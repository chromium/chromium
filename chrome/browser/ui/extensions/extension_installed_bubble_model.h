// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_MODEL_H_

#include <string>

#include "extensions/common/extension_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace extensions {
class Extension;
}  // namespace extensions

// An ExtensionInstalledBubbleModel represents the state of an
// "extension installed" bubble. Instances of this class are entirely immutable
// after construction.
class ExtensionInstalledBubbleModel {
 public:
  ExtensionInstalledBubbleModel(Profile* profile,
                                const extensions::Extension* extension,
                                const SkBitmap& icon);
  ~ExtensionInstalledBubbleModel();
  ExtensionInstalledBubbleModel(const ExtensionInstalledBubbleModel& other) =
      delete;
  ExtensionInstalledBubbleModel& operator=(
      const ExtensionInstalledBubbleModel& other) = delete;

  bool anchor_to_action() const { return anchor_to_action_; }
  bool anchor_to_omnibox() const { return anchor_to_omnibox_; }

  bool show_how_to_use() const { return show_how_to_use_; }
  bool show_how_to_manage() const { return show_how_to_manage_; }
  bool show_key_binding() const { return show_key_binding_; }
  bool show_sign_in_promo() const { return show_sign_in_promo_; }

  std::u16string GetHowToUseText() const;

  gfx::ImageSkia MakeIconOfSize(const gfx::Size& size) const;

  const extensions::ExtensionId& extension_id() const { return extension_id_; }
  const std::string& extension_name() const { return extension_name_; }

 private:
  // Whether the install bubble should anchor to the extension's action button
  // or to the omnibox.  At most one of these is true.
  bool anchor_to_action_ = false;
  bool anchor_to_omnibox_ = false;

  // Whether to show the how-to-use and how-to-manage text in the install
  // bubble.
  bool show_how_to_use_ = false;
  bool show_how_to_manage_ = false;

  // Whether to show the extension's key binding in the install bubble.
  bool show_key_binding_ = false;

  // Whether to show a signin promo in the install bubble.
  bool show_sign_in_promo_ = false;

  std::u16string how_to_use_text_;

  const SkBitmap icon_;

  const extensions::ExtensionId extension_id_;
  const std::string extension_name_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_MODEL_H_
