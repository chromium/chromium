// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_ENTRY_UI_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_ENTRY_UI_H_

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"

enum class DeviceEntryUIType {
  kAudio = 0,
  kCast = 1,
  kMaxValue = kCast,
};

class DeviceEntryUI {
 public:
  DeviceEntryUI(const std::string& raw_device_id,
                const std::string& device_name,
                const gfx::VectorIcon* icon_,
                const std::string& subtext = "");

  DeviceEntryUI(const DeviceEntryUI&) = delete;
  DeviceEntryUI& operator=(const DeviceEntryUI&) = delete;
  virtual ~DeviceEntryUI() = default;

  const std::string& raw_device_id() const { return raw_device_id_; }
  const std::string& device_name() const { return device_name_; }

  virtual void OnColorsChanged(SkColor foreground_color,
                               SkColor background_color) = 0;
  virtual DeviceEntryUIType GetType() const = 0;

  bool GetEntryIsHighlightedForTesting() const { return is_highlighted_; }

 protected:
  const std::string raw_device_id_;
  const std::string device_name_;
  bool is_highlighted_ = false;
  const gfx::VectorIcon* const icon_;
};

class AudioDeviceEntryView : public DeviceEntryUI, public HoverButton {
 public:
  AudioDeviceEntryView(views::ButtonListener* button_listener,
                       SkColor foreground_color,
                       SkColor background_color,
                       const std::string& raw_device_id,
                       const std::string& name,
                       const std::string& subtext = "");
  ~AudioDeviceEntryView() override = default;

  // DeviceEntryUI
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override;
  DeviceEntryUIType GetType() const override;

  // HoverButton
  SkColor GetInkDropBaseColor() const override;

  void SetHighlighted(bool highlighted);
};

class CastDeviceEntryView : public DeviceEntryUI,
                            public media_router::CastDialogSinkButton {
 public:
  CastDeviceEntryView(views::ButtonListener* button_listener,
                      SkColor foreground_color,
                      SkColor background_color,
                      const media_router::UIMediaSink& sink);
  ~CastDeviceEntryView() override = default;

  // DeviceEntryUI
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override;
  DeviceEntryUIType GetType() const override;

  // HoverButton
  SkColor GetInkDropBaseColor() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_ENTRY_UI_H_
