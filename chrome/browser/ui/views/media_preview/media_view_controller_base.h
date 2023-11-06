// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_VIEW_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_VIEW_CONTROLLER_BASE_H_

#include <stddef.h>

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  // `on_selection_changed` runs on a combobox selection.
  MediaViewControllerBase(MediaView& base_view,
                          bool needs_borders,
                          ui::ComboboxModel* model,
                          base::RepeatingClosure on_selection_changed,
                          const std::u16string& combobox_accessible_name,
                          const std::u16string& no_device_connected_label_text);
  MediaViewControllerBase(const MediaViewControllerBase&) = delete;
  MediaViewControllerBase& operator=(const MediaViewControllerBase&) = delete;
  ~MediaViewControllerBase();

  // Returns the immediate parent view of the live camera/mic feeds.
  MediaView& GetLiveFeedContainer();

  // Enables the combobox if there are connected devices (e.g.`has_devices` is
  // true).
  void AdjustComboboxEnabledState(bool has_devices);

  absl::optional<size_t> GetComboboxSelectedIndex() const;

  // Updates `active_device_id_` to `device_id`.
  // Returns false, if `active_device_id_` is already equal to `device_id`.
  // Otherwise returns true.
  bool UpdateActiveDeviceId(const std::string& device_id);

 private:
  friend class MediaViewControllerBaseTest;

  const raw_ref<MediaView> base_view_;
  const raw_ref<views::Label> no_device_connected_label_;
  const raw_ref<views::Combobox> device_selector_combobox_;

  std::string active_device_id_;
  const base::RepeatingClosure combobox_selection_change_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_VIEW_CONTROLLER_BASE_H_
