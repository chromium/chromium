// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_COLOR_CHOOSER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_COLOR_CHOOSER_MAC_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "components/remote_cocoa/app_shim/color_panel_bridge.h"
#include "components/remote_cocoa/common/color_panel.mojom.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class ColorChooserMac : public content::ColorChooser,
                        public remote_cocoa::mojom::ColorPanelHost {
 public:
  ColorChooserMac(const ColorChooserMac&) = delete;
  ColorChooserMac& operator=(const ColorChooserMac&) = delete;

  // Returns a ColorChooserMac instance.
  // The returned instance must be owned by the passed in web_contents.
  // The instance will be freed when calling End(). The instance accomplishes
  // this by making calls to the passed in web_contents.
  // Open() returns a new instance after freeing the previous one (i.e. it does
  // not reuse the previous instance even if it still exists).
  // TODO(crbug.com/1294002): Refactor ColorChooserMac and WebContents
  // interactions
  static std::unique_ptr<ColorChooserMac> Create(
      content::WebContents* web_contents,
      SkColor initial_color,
      remote_cocoa::ColorPanelBridge::ShowCallback callback);

  ~ColorChooserMac() override;

  // content::ColorChooser.
  void SetSelectedColor(SkColor color) override;
  void End() override;

  // content::ColorChooser variation that has a callback.
  void SetSelectedColor(
      SkColor color,
      remote_cocoa::ColorPanelBridge::SetSelectedColorCallback callback);

 private:
  // remote_cocoa::mojom::ColorPanelHost.
  void DidChooseColorInColorPanel(SkColor color) override;
  void DidCloseColorPanel() override;

  ColorChooserMac(content::WebContents* tab,
                  SkColor initial_color,
                  remote_cocoa::ColorPanelBridge::ShowCallback callback);

  // The web contents invoking the color chooser.  No ownership because it will
  // outlive this class.
  raw_ptr<content::WebContents> web_contents_;

  mojo::Remote<remote_cocoa::mojom::ColorPanel> mojo_panel_remote_;
  mojo::Receiver<remote_cocoa::mojom::ColorPanelHost> mojo_host_receiver_{this};
};

#endif  // CHROME_BROWSER_UI_COCOA_COLOR_CHOOSER_MAC_H_
