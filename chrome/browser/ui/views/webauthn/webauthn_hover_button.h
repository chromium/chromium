// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_HOVER_BUTTON_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
class Label;
class View;
}  // namespace views

// WebauthnHoverButton is a hoverable button with a primary left-hand icon, a
// title and subtitle, and a secondary right-hand icon (usually a submenu
// arrow). Icons and subtitle are optional.
class WebAuthnHoverButton : public HoverButton {
  METADATA_HEADER(WebAuthnHoverButton, HoverButton)

 public:
  // Creates a hoverable button with the given elements, like so:
  //
  // +-------------------------------------------------------------------+
  // |      |    title                                            |second|
  // | icon |                                                     |ary_ic|
  // |      |    subtitle                                         |  on  |
  // +-------------------------------------------------------------------+
  //
  // If |subtitle| is omitted and |force_two_line| is false, the button shrinks
  // to a single-line automatically; if |force_two_line| is true, |title| is
  // horizontally centered over the two-line height.
  //
  // |icon| and |secondary_icon| are also optional. If either is null, the
  // middle column resizes to fill the space.
  WebAuthnHoverButton(PressedCallback callback,
                      std::unique_ptr<views::ImageView> icon,
                      const std::u16string& title,
                      const std::u16string& subtitle,
                      std::unique_ptr<views::View> secondary_icon,
                      bool enabled);
  WebAuthnHoverButton(const WebAuthnHoverButton&) = delete;
  WebAuthnHoverButton& operator=(const WebAuthnHoverButton&) = delete;
  ~WebAuthnHoverButton() override = default;

 private:
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
  raw_ptr<views::View> icon_view_ = nullptr;
  raw_ptr<views::View> secondary_icon_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_HOVER_BUTTON_H_
