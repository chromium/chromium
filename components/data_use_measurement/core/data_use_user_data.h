// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_USER_DATA_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_USER_DATA_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/supports_user_data.h"

namespace data_use_measurement {

// Used to annotate URLRequests with the service name if the URLRequest is used
// by a service.
class DataUseUserData : public base::SupportsUserData::Data {
 public:
  // Data use broken by content type. This enum must remain synchronized
  // with the enum of the same name in metrics/histograms/histograms.xml.
  // These values are written to logs.  New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum DataUseContentType {
    OTHER = 0,
    MAIN_FRAME_HTML = 1,
    NON_MAIN_FRAME_HTML = 2,
    CSS = 3,
    IMAGE = 4,
    JAVASCRIPT = 5,
    FONT = 6,
    AUDIO_APPBACKGROUND = 7,
    AUDIO_TABBACKGROUND = 8,
    AUDIO = 9,
    VIDEO_APPBACKGROUND = 10,
    VIDEO_TABBACKGROUND = 11,
    VIDEO = 12,
    kMaxValue = 13,
  };

  // The state of the application. Only available on Android and on other
  // platforms it is always FOREGROUND.
  enum AppState { UNKNOWN, BACKGROUND, FOREGROUND };

  explicit DataUseUserData(AppState app_state);
  ~DataUseUserData() override;

  AppState app_state() const { return app_state_; }

  void set_app_state(AppState app_state) { app_state_ = app_state; }

  DataUseContentType content_type() { return content_type_; }

  void set_content_type(DataUseContentType content_type) {
    content_type_ = content_type;
  }

  // The key for retrieving back this type of user data.
  static const void* const kUserDataKey;

 private:
  // App state when network access was performed for the request previously.
  AppState app_state_;

  DataUseContentType content_type_;

  DISALLOW_COPY_AND_ASSIGN(DataUseUserData);
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_USER_DATA_H_
