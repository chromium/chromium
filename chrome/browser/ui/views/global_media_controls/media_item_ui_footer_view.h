// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_FOOTER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_FOOTER_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_observer.h"
#include "components/global_media_controls/public/views/media_item_ui_footer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/button/button.h"

namespace {
class DeviceEntryButton;
}  // anonymous namespace

// A footer view attached to MediaItemUIView containing
// available cast devices and volume controls.
class MediaItemUIFooterView : public global_media_controls::MediaItemUIFooter,
                              public MediaItemUIDeviceSelectorObserver {
  METADATA_HEADER(MediaItemUIFooterView,
                  global_media_controls::MediaItemUIFooter)
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnDeviceSelected(int tag) = 0;
    virtual void OnDropdownButtonClicked() = 0;
    virtual bool IsDeviceSelectorExpanded() = 0;
  };

  explicit MediaItemUIFooterView(base::RepeatingClosure stop_casting_callback);
  ~MediaItemUIFooterView() override;

  // global_media_controls::MediaItemUIFooter:
  void OnColorsChanged(SkColor foreground, SkColor background) override;

  void SetDelegate(Delegate* delegate);

  // MediaItemDeviceSelectorObserver:
  void OnMediaItemUIDeviceSelectorUpdated(
      const std::map<int, raw_ptr<DeviceEntryUI, CtnExperimental>>&
          device_entries_map) override;

  void Layout(PassKey) override;

 private:
  void UpdateButtonsColor();
  void OnDeviceSelected(int tag);
  void OnOverflowButtonClicked();

  SkColor foreground_color_ = gfx::kPlaceholderColor;

  raw_ptr<DeviceEntryButton, DanglingUntriaged> overflow_button_ = nullptr;

  raw_ptr<Delegate> delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_FOOTER_VIEW_H_
