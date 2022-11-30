// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixers.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixer.h"
#include "chrome/browser/ui/color/native_chrome_color_mixer.h"
#include "chrome/browser/ui/color/new_tab_page_color_mixer.h"
#include "chrome/browser/ui/color/omnibox_color_mixer.h"
#include "chrome/browser/ui/color/tab_strip_color_mixer.h"
#include "ui/color/color_provider_utils.h"

namespace {

class ChromeColorProviderUtilsCallbacks
    : public ui::ColorProviderUtilsCallbacks {
 public:
  bool ColorIdName(ui::ColorId color_id,
                   base::StringPiece* color_name) override;
};

#include "ui/color/color_id_map_macros.inc"

bool ChromeColorProviderUtilsCallbacks::ColorIdName(
    ui::ColorId color_id,
    base::StringPiece* color_name) {
  static constexpr const auto chrome_color_id_map =
      base::MakeFixedFlatMap<ui::ColorId, const char*>({CHROME_COLOR_IDS});
  auto* i = chrome_color_id_map.find(color_id);
  if (i != chrome_color_id_map.cend()) {
    *color_name = i->second;
    return true;
  }
  return false;
}

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_map_macros.inc"

}  // namespace

void AddChromeColorMixers(ui::ColorProvider* provider,
                          const ui::ColorProviderManager::Key& key) {
  static base::NoDestructor<ChromeColorProviderUtilsCallbacks>
      chrome_color_provider_utils_callbacks;
  ui::SetColorProviderUtilsCallbacks(
      chrome_color_provider_utils_callbacks.get());
  AddChromeColorMixer(provider, key);
  AddOmniboxColorMixer(provider, key);
  AddTabStripColorMixer(provider, key);
  AddNewTabPageColorMixer(provider, key);

  // Must be the last one in order to override other mixer colors.
  AddNativeChromeColorMixer(provider, key);

  if (key.custom_theme) {
    key.custom_theme->AddColorMixers(provider, key);
  }

  if (key.app_controller) {
    key.app_controller->AddColorMixers(provider, key);
  }
}
