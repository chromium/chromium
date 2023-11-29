// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_VIEW_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_VIEW_CONTROLLER_BASE_H_

#include <stddef.h>

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"

class MediaView;

namespace views {
class Combobox;
class Label;
}  // namespace views

namespace ui {
class ComboboxModel;
}  // namespace ui

// This class encapsulates common view logic for both camera and mic media
// views.
class MediaViewControllerBase {
 public:
  // Gets the combobox selected camera / mic index.
  using SourceChangeCallback =
      base::RepeatingCallback<void(std::optional<size_t> selected_index)>;

  // `source_change_callback` runs on a combobox selection.
  MediaViewControllerBase(MediaView& base_view,
                          bool needs_borders,
                          ui::ComboboxModel* model,
                          SourceChangeCallback source_change_callback,
                          const std::u16string& combobox_accessible_name,
                          const std::u16string& no_device_connected_label_text);
  MediaViewControllerBase(const MediaViewControllerBase&) = delete;
  MediaViewControllerBase& operator=(const MediaViewControllerBase&) = delete;
  ~MediaViewControllerBase();

  // Returns the immediate parent view of the live camera/mic feeds.
  MediaView& GetLiveFeedContainer() { return live_feed_container_.get(); }

  // Enables the combobox if there are connected devices (e.g.`has_devices` is
  // true).
  void AdjustComboboxEnabledState(bool has_devices);

 private:
  friend class MediaViewControllerBaseTest;

  void OnComboboxSelection();

  const raw_ref<MediaView> base_view_;
  const raw_ref<MediaView> live_feed_container_;
  const raw_ref<views::Label> no_device_connected_label_;
  const raw_ref<views::Combobox> device_selector_combobox_;

  const SourceChangeCallback source_change_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_VIEW_CONTROLLER_BASE_H_
