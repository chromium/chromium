// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_VIEW_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/media_preview/media_view_controller_base.h"

class MediaView;
class MicSelectorComboboxModel;
struct AudioSourceInfo;

// The MediaViewController for the mic view. It sets up the mic view, and
// updates it based on the data it receives from the Coordinator. Also it
// notifies the coordinator of changes resulting from events on the view.
class MicViewController {
 public:
  MicViewController(MediaView& base_view,
                    bool needs_borders,
                    MicSelectorComboboxModel& combobox_model,
                    MediaViewControllerBase::SourceChangeCallback callback);
  MicViewController(const MicViewController&) = delete;
  MicViewController& operator=(const MicViewController&) = delete;
  ~MicViewController();

  // Returns the immediate parent view of the live mic feed.
  MediaView& GetLiveFeedContainer();

  // `audio_source_infos` is  a list of connected devices. When a new device
  // gets connected or a device gets disconnected, this function is called to
  // update the list of devices in the combobox.
  void UpdateAudioSourceInfos(std::vector<AudioSourceInfo> audio_source_infos);

 private:
  const raw_ref<MicSelectorComboboxModel> combobox_model_;
  std::unique_ptr<MediaViewControllerBase> base_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_MIC_VIEW_CONTROLLER_H_
