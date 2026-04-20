// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font_data/dwrite_local_font_matcher.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/win/direct_write.h"

namespace font_data_service {

DWriteLocalFontMatcher::DWriteLocalFontMatcher() = default;

DWriteLocalFontMatcher::~DWriteLocalFontMatcher() = default;

std::optional<LocalFontMatchResult> DWriteLocalFontMatcher::MatchLocalFont(
    const std::string& font_unique_name) {
  TRACE_EVENT("fonts", "DWriteLocalFontMatcher::MatchLocalFont",
              "font_unique_name", font_unique_name);

  EnsureDirectWriteInitialized();

  if (!system_font_set_) {
    return std::nullopt;
  }

  std::wstring wide_name = base::UTF8ToWide(font_unique_name);
  Microsoft::WRL::ComPtr<IDWriteFontSet> filtered_set;
  auto filter_set = [&filtered_set, &wide_name, &font_set = system_font_set_](
                        DWRITE_FONT_PROPERTY_ID property_id) {
    DWRITE_FONT_PROPERTY search_property = {property_id, wide_name.c_str(),
                                            L""};
    HRESULT hr = font_set->GetMatchingFonts(&search_property, 1, &filtered_set);
    return SUCCEEDED(hr);
  };

  bool found = filter_set(DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME) &&
               filtered_set->GetFontCount();
  if (!found) {
    found = filter_set(DWRITE_FONT_PROPERTY_ID_FULL_NAME) &&
            filtered_set->GetFontCount();
  }

  if (!found) {
    return std::nullopt;
  }

  return GetFileInfoFromFilteredSet(filtered_set);
}

std::optional<LocalFontMatchResult>
DWriteLocalFontMatcher::GetFileInfoFromFilteredSet(
    const Microsoft::WRL::ComPtr<IDWriteFontSet>& filtered_set) {
  Microsoft::WRL::ComPtr<IDWriteFontFaceReference> first_font_ref;
  HRESULT hr = filtered_set->GetFontFaceReference(0, &first_font_ref);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  Microsoft::WRL::ComPtr<IDWriteFontFace3> font_face;
  hr = first_font_ref->CreateFontFace(&font_face);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  UINT32 file_count = 0;
  hr = font_face->GetFiles(&file_count, nullptr);
  if (FAILED(hr) || file_count != 1) {
    return std::nullopt;
  }

  Microsoft::WRL::ComPtr<IDWriteFontFile> font_file;
  hr = font_face->GetFiles(&file_count, &font_file);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  Microsoft::WRL::ComPtr<IDWriteFontFileLoader> loader;
  hr = font_file->GetLoader(&loader);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  Microsoft::WRL::ComPtr<IDWriteLocalFontFileLoader> local_loader;
  hr = loader.As(&local_loader);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  const void* key = nullptr;
  UINT32 key_size = 0;
  hr = font_file->GetReferenceKey(&key, &key_size);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  UINT32 path_length = 0;
  hr = local_loader->GetFilePathLengthFromKey(key, key_size, &path_length);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  std::wstring file_path;
  ++path_length;  // Reserve space for the null terminator.
  file_path.resize(path_length);
  hr = local_loader->GetFilePathFromKey(key, key_size, &file_path[0],
                                        path_length);
  if (FAILED(hr)) {
    return std::nullopt;
  }
  --path_length;  // Remove null terminator from std::wstring.
  file_path.resize(path_length);

  return LocalFontMatchResult{base::FilePath(file_path),
                              static_cast<int>(font_face->GetIndex())};
}

void DWriteLocalFontMatcher::EnsureDirectWriteInitialized() {
  if (direct_write_initialized_) {
    return;
  }
  direct_write_initialized_ = true;

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  gfx::win::CreateDWriteFactory(&factory);
  if (!factory) {
    return;
  }

  Microsoft::WRL::ComPtr<IDWriteFactory3> factory3;
  factory.As(&factory3);
  if (!factory3) {
    return;
  }

  GetLocalFontSet(factory3);
}

void DWriteLocalFontMatcher::GetLocalFontSet(
    const Microsoft::WRL::ComPtr<IDWriteFactory3>& factory3) {
  const std::vector<base::FilePath>* sideloaded_fonts =
      gfx::win::GetSideloadedFontsForTesting();  // IN-TEST
  if (!sideloaded_fonts) {
    HRESULT hr = factory3->GetSystemFontSet(&system_font_set_);
    if (FAILED(hr)) {
      system_font_set_ = nullptr;
    }
    return;
  }

  Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> font_set_builder;
  HRESULT hr = factory3->CreateFontSetBuilder(&font_set_builder);
  if (FAILED(hr)) {
    return;
  }

  for (const auto& path : *sideloaded_fonts) {
    Microsoft::WRL::ComPtr<IDWriteFontFile> font_file;
    hr = factory3->CreateFontFileReference(path.value().c_str(), nullptr,
                                           &font_file);
    if (FAILED(hr)) {
      continue;
    }

    BOOL supported;
    DWRITE_FONT_FILE_TYPE file_type;
    UINT32 n_fonts;
    hr = font_file->Analyze(&supported, &file_type, nullptr, &n_fonts);
    if (FAILED(hr)) {
      continue;
    }

    for (UINT32 font_index = 0; font_index < n_fonts; ++font_index) {
      Microsoft::WRL::ComPtr<IDWriteFontFaceReference> font_face;
      hr = factory3->CreateFontFaceReference(font_file.Get(), font_index,
                                             DWRITE_FONT_SIMULATIONS_NONE,
                                             &font_face);
      if (FAILED(hr)) {
        continue;
      }
      font_set_builder->AddFontFaceReference(font_face.Get());
    }
  }

  Microsoft::WRL::ComPtr<IDWriteFontSet> system_font_set;
  hr = factory3->GetSystemFontSet(&system_font_set);
  if (FAILED(hr)) {
    return;
  }

  hr = font_set_builder->AddFontSet(system_font_set.Get());
  if (FAILED(hr)) {
    return;
  }

  hr = font_set_builder->CreateFontSet(&system_font_set_);
  if (FAILED(hr)) {
    system_font_set_ = nullptr;
  }
}

}  // namespace font_data_service
