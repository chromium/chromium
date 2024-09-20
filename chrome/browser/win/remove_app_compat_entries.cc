// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/remove_app_compat_entries.h"

#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"

void RemoveAppCompatEntries(const base::FilePath& program) {
  if (!program.IsAbsolute()) {
    return;
  }

  base::win::RegKey key;

  // Read the current layers value and parse it into tokens.
  std::wstring layers;
  if (key.Open(HKEY_CURRENT_USER,
               L"Software\\Microsoft\\Windows "
               L"NT\\CurrentVersion\\AppCompatFlags\\Layers",
               KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
      key.ReadValue(program.value().c_str(), &layers) != ERROR_SUCCESS ||
      layers.empty()) {
    return;
  }

  if (RemoveCompatLayers(layers)) {
    if (layers.empty()) {
      key.DeleteValue(program.value().c_str());
    } else {
      key.WriteValue(program.value().c_str(), layers.c_str());
    }
  }
}

bool RemoveCompatLayers(std::wstring& layers) {
  std::vector<std::wstring_view> tokens = base::SplitStringPiece(
      layers, L" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty()) {
    return false;  // Nothing to remove.
  }

  static constexpr std::wstring_view kMagicToken(L"~");
  if (tokens.size() == 1 && tokens.front() == kMagicToken) {
    // The input is nothing but the leading token.
    layers.clear();
    return true;
  }

  // Remove all known compatibility mode layer names.
  static constexpr auto kCompatModeTokens =
      base::MakeFixedFlatSet<std::wstring_view>({
          L"NT4SP5",      L"VISTARTM",    L"VISTASP1",    L"VISTASP2",
          L"WIN10RTM",    L"WIN2000",     L"WIN4SP5",     L"WIN7RTM",
          L"WIN7SP1",     L"WIN8RTM",     L"WIN81RTM",    L"WIN95",
          L"WIN98",       L"WINSRV03SP1", L"WINSRV08SP1", L"WINSRV16RTM",
          L"WINSRV19RTM", L"WINXPSP2",    L"WINXPSP3",
      });
  const auto new_end =
      base::ranges::remove_if(tokens, [](const std::wstring_view& token) {
        return kCompatModeTokens.contains(token);
      });
  if (new_end == tokens.end()) {
    return false;  // No changes made.
  }
  tokens.erase(new_end, tokens.end());
  if (tokens.empty() || (tokens.size() == 1 && tokens.front() == kMagicToken)) {
    layers.clear();
  } else {
    layers = base::JoinString(tokens, L" ");
  }
  return true;
}
