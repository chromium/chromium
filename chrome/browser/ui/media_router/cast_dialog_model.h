// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_DIALOG_MODEL_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_DIALOG_MODEL_H_

#include <optional>
#include <string>

#include "chrome/browser/ui/media_router/ui_media_sink.h"

namespace media_router {

// Holds data needed to populate a Cast dialog.
class CastDialogModel {
 public:
  CastDialogModel();
  CastDialogModel(const CastDialogModel& other);
  ~CastDialogModel();

  // Returns the index of the first sink with an active route, or nullopt if
  // there is no such sink.
  std::optional<size_t> GetFirstActiveSinkIndex() const;

  void set_dialog_header(const std::u16string& dialog_header) {
    dialog_header_ = dialog_header;
  }
  const std::u16string& dialog_header() const { return dialog_header_; }

  void set_media_sinks(const std::vector<UIMediaSink>& media_sinks) {
    media_sinks_ = media_sinks;
  }
  const std::vector<UIMediaSink>& media_sinks() const { return media_sinks_; }

  void set_is_permission_rejected(bool is_permission_rejected) {
    is_permission_rejected_ = is_permission_rejected;
  }
  bool is_permission_rejected() const { return is_permission_rejected_; }

 private:
  // The header to use at the top of the dialog.
  // This reflects the current activity associated with the tab.
  std::u16string dialog_header_;

  // Sink data in the order they should be shown in the dialog.
  std::vector<UIMediaSink> media_sinks_;

  // Whether the dialog should show a local discovery permission rejected error.
  bool is_permission_rejected_ = false;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_DIALOG_MODEL_H_
