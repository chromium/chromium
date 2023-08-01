// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/color_chooser_mac.h"

#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/color_chooser.h"
#include "components/remote_cocoa/app_shim/color_panel_bridge.h"
#include "components/remote_cocoa/browser/application_host.h"
#include "components/remote_cocoa/browser/window.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "skia/ext/skia_utils_mac.h"

namespace {
// The currently active color chooser.
ColorChooserMac* g_current_color_chooser = nullptr;
}  // namespace

// static
std::unique_ptr<ColorChooserMac> ColorChooserMac::Create(
    content::WebContents* web_contents,
    SkColor initial_color,
    remote_cocoa::ColorPanelBridge::ShowCallback callback) {
  if (g_current_color_chooser)
    g_current_color_chooser->End();
  DCHECK(!g_current_color_chooser);
  // Note that WebContentsImpl::ColorChooser ultimately takes ownership (and
  // deletes) the returned pointer.
  g_current_color_chooser =
      new ColorChooserMac(web_contents, initial_color, std::move(callback));
  return base::WrapUnique(g_current_color_chooser);
}

ColorChooserMac::ColorChooserMac(
    content::WebContents* web_contents,
    SkColor initial_color,
    remote_cocoa::ColorPanelBridge::ShowCallback callback)
    : web_contents_(web_contents) {
  // The application_host branch is used when running as a PWA.
  auto* application_host = remote_cocoa::ApplicationHost::GetForNativeView(
      web_contents ? web_contents->GetNativeView() : gfx::NativeView());
  if (application_host) {
    application_host->GetApplication()->ShowColorPanel(
        mojo_panel_remote_.BindNewPipeAndPassReceiver(),
        mojo_host_receiver_.BindNewPipeAndPassRemote());
  } else {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<remote_cocoa::ColorPanelBridge>(
            mojo_host_receiver_.BindNewPipeAndPassRemote()),
        mojo_panel_remote_.BindNewPipeAndPassReceiver());
  }
  mojo_panel_remote_->Show(initial_color, std::move(callback));
}

ColorChooserMac::~ColorChooserMac() {
  // Always call End() before destroying.
  DCHECK_NE(g_current_color_chooser, this);
}

void ColorChooserMac::DidChooseColorInColorPanel(SkColor color) {
  DCHECK_EQ(g_current_color_chooser, this);
  if (web_contents_)
    web_contents_->DidChooseColorInColorChooser(color);
}

void ColorChooserMac::DidCloseColorPanel() {
  DCHECK_EQ(g_current_color_chooser, this);
  End();
}

void ColorChooserMac::End() {
  if (g_current_color_chooser == this) {
    g_current_color_chooser = nullptr;
    if (web_contents_)
      web_contents_->DidEndColorChooser();
  }
}

void ColorChooserMac::SetSelectedColor(SkColor color) {
  SetSelectedColor(color, base::DoNothing());
}

void ColorChooserMac::SetSelectedColor(
    SkColor color,
    remote_cocoa::ColorPanelBridge::SetSelectedColorCallback callback) {
  mojo_panel_remote_->SetSelectedColor(color, std::move(callback));
}

namespace chrome {
std::unique_ptr<content::ColorChooser> ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return ColorChooserMac::Create(web_contents, initial_color,
                                 base::DoNothing());
}
}  // namespace chrome
