// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "components/bubble/bubble_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"

class Browser;

namespace extensions {
struct ActionInfo;
class Command;
class Extension;
}

namespace gfx {
class Point;
}

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the Bubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browser action icon in the toolbar.
//    PAGE_ACTION    -> A preview of the page action icon in the location
//                      bar which is shown while the Bubble is shown.
//    GENERIC        -> The app menu. This case includes page actions that
//                      don't specify a default icon.
// NB: This bubble is using the temporarily-deprecated bubble manager interface
// BubbleUi. Do not copy this pattern.
class ExtensionInstalledBubble : public BubbleDelegate {
 public:
  // The behavior and content of this Bubble comes in these varieties:
  enum BubbleType {
    OMNIBOX_KEYWORD,
    BROWSER_ACTION,
    PAGE_ACTION,
    GENERIC
  };

  // The different options to show in the installed bubble.
  enum Options {
    NONE = 0,
    HOW_TO_USE = 1 << 0,
    HOW_TO_MANAGE = 1 << 1,
    SHOW_KEYBINDING = 1 << 2,
    SIGN_IN_PROMO = 1 << 3,
  };

  // The different possible anchor positions.
  enum AnchorPosition {
    ANCHOR_ACTION,
    ANCHOR_OMNIBOX,
    ANCHOR_APP_MENU,
  };

  // Creates the ExtensionInstalledBubble and schedules it to be shown once
  // the extension has loaded. |extension| is the installed extension. |browser|
  // is the browser window which will host the bubble. |icon| is the install
  // icon of the extension.
  static void ShowBubble(scoped_refptr<const extensions::Extension> extension,
                         Browser* browser,
                         const SkBitmap& icon);

  ExtensionInstalledBubble(scoped_refptr<const extensions::Extension> extension,
                           Browser* browser,
                           const SkBitmap& icon);

  ~ExtensionInstalledBubble() override;

  const extensions::Extension* extension() const { return extension_.get(); }
  Browser* browser() { return browser_; }
  const Browser* browser() const { return browser_; }
  const SkBitmap& icon() const { return icon_; }
  BubbleType type() const { return type_; }
  bool has_command_keybinding() const { return !!action_command_; }
  int options() const { return options_; }
  AnchorPosition anchor_position() const { return anchor_position_; }

  // BubbleDelegate:
  std::unique_ptr<BubbleUi> BuildBubbleUi() override;
  bool ShouldClose(BubbleCloseReason reason) const override;
  std::string GetName() const override;
  const content::RenderFrameHost* OwningFrame() const override;

  // Returns false if the bubble could not be shown immediately, because of an
  // animation (eg. adding a new browser action to the toolbar).
  // TODO(hcarmona): Detect animation in a platform-agnostic manner.
  bool ShouldShow();

  // Returns the anchor point in screen coordinates. Used when there is no
  // anchor view.
  gfx::Point GetAnchorPoint(gfx::NativeWindow window) const;

  // Returns the string describing how to use the new extension.
  base::string16 GetHowToUseDescription() const;

 private:
  ExtensionInstalledBubble(scoped_refptr<const extensions::Extension> extension,
                           Browser* browser,
                           const SkBitmap& icon,
                           const extensions::ActionInfo* action_info);

  // It's possible for an extension to be programmatically uninstalled
  // underneath us, so don't let the extension object go away until the bubble
  // is hidden.
  const scoped_refptr<const extensions::Extension> extension_;
  Browser* const browser_;
  const SkBitmap icon_;
  const BubbleType type_;

  // The command to execute the extension action, if one exists.
  const std::unique_ptr<extensions::Command> action_command_;

  // A bitmask containing the various options of bubble sections to show.
  const int options_;

  // The location where the bubble should be anchored.
  const AnchorPosition anchor_position_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubble);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_
