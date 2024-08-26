// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_ENTRY_UI_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_ENTRY_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"

enum class DeviceEntryUIType {
  kAudio = 0,
  kCast = 1,
  kMaxValue = kCast,
};

class DeviceEntryUI {
 public:
  DeviceEntryUI(const std::string& raw_device_id,
                const std::string& device_name,
                const gfx::VectorIcon& icon,
                const std::string& subtext = "");

  DeviceEntryUI(const DeviceEntryUI&) = delete;
  DeviceEntryUI& operator=(const DeviceEntryUI&) = delete;
  virtual ~DeviceEntryUI() = default;

  const gfx::VectorIcon& icon() const { return *icon_; }
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
  const raw_ref<const gfx::VectorIcon> icon_;
};

class AudioDeviceEntryView : public DeviceEntryUI, public HoverButton {
  METADATA_HEADER(AudioDeviceEntryView, HoverButton)

 public:
  AudioDeviceEntryView(PressedCallback callback,
                       SkColor foreground_color,
                       SkColor background_color,
                       const std::string& raw_device_id,
                       const std::string& name);
  ~AudioDeviceEntryView() override = default;

  // DeviceEntryUI
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override;
  DeviceEntryUIType GetType() const override;

  void SetHighlighted(bool highlighted);
  bool GetHighlighted() const;
};

class CastDeviceEntryView : public DeviceEntryUI, public HoverButton {
  METADATA_HEADER(CastDeviceEntryView, HoverButton)

 public:
  CastDeviceEntryView(base::RepeatingClosure callback,
                      SkColor foreground_color,
                      SkColor background_color,
                      const global_media_controls::mojom::DevicePtr& device);
  ~CastDeviceEntryView() override;

  // DeviceEntryUI
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override;
  DeviceEntryUIType GetType() const override;

  std::string GetStatusTextForTest() const;

 private:
  void ChangeCastEntryColor(SkColor foreground_color, SkColor background_color);

  global_media_controls::mojom::DevicePtr device_;
};

// This media cast device entry UI only shows on Chrome OS ash.
class CastDeviceEntryViewAsh : public DeviceEntryUI, public HoverButton {
  METADATA_HEADER(CastDeviceEntryViewAsh, HoverButton)

 public:
  CastDeviceEntryViewAsh(PressedCallback callback,
                         ui::ColorId foreground_color_id,
                         ui::ColorId background_color_id,
                         const global_media_controls::mojom::DevicePtr& device);
  ~CastDeviceEntryViewAsh() override;

  // DeviceEntryUI
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override {}
  DeviceEntryUIType GetType() const override;

 private:
  global_media_controls::mojom::DevicePtr device_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_ENTRY_UI_H_
