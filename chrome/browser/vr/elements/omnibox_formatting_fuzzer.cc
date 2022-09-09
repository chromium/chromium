// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/elements/omnibox_formatting.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/render_text_test_api.h"
#include "url/gurl.h"

struct Setup {
  Setup() {
    base::CommandLine::Init(0, nullptr);
    gfx::FontList::SetDefaultFontDescription("Arial, Times New Roman, 15px");
    CHECK(base::i18n::InitializeICU());
  }
  base::AtExitManager at_exit_manager;
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Setup setup;

  FuzzedDataProvider data_provider(data, size);
  const int field_width = data_provider.ConsumeIntegral<int>();
  const int character_width = data_provider.ConsumeIntegral<int>();
  const int min_path_pixels = data_provider.ConsumeIntegral<int>();
  const bool cursor_enabled = data_provider.ConsumeBool();

  // Using maximum enum values for |gfx::HorizontalAlignment| and
  // |gfx::DirectionalityMode|, respectively.
  const auto horizontal_alignment = static_cast<enum gfx::HorizontalAlignment>(
      data_provider.ConsumeIntegralInRange<int>(0, gfx::ALIGN_TO_HEAD));
  const auto directionality_mode = static_cast<enum gfx::DirectionalityMode>(
      data_provider.ConsumeIntegralInRange<int>(0, gfx::DIRECTIONALITY_AS_URL));

  const std::string url_spec =
      data_provider.ConsumeBytesAsString(data_provider.remaining_bytes());

  GURL gurl(base::UTF8ToUTF16(url_spec));
  if (!gurl.is_valid())
    return 0;

  url::Parsed parsed;
  const std::u16string text = vr::FormatUrlForVr(gurl, &parsed);
  CHECK(text.length());

  gfx::FontList font_list;
  gfx::Rect field(field_width, font_list.GetHeight());

  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  render_text->SetFontList(font_list);
  render_text->SetHorizontalAlignment(horizontal_alignment);
  render_text->SetDirectionalityMode(directionality_mode);
  render_text->SetText(text);
  render_text->SetDisplayRect(field);
  render_text->SetCursorEnabled(cursor_enabled);

  gfx::test::RenderTextTestApi render_text_test_api(render_text.get());
  render_text_test_api.SetGlyphWidth(character_width);

  vr::GetElisionParameters(gurl, parsed, render_text.get(), min_path_pixels);

  return 0;
}
