// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_APP_TYPES_H_
#define CHROMEOS_UI_BASE_APP_TYPES_H_

namespace chromeos {

// App type of the window.
// This enum is used to control a UMA histogram buckets. If you change this
// enum, you should update DownEventMetric in
// ash/metrics/pointer_metrics_recorder.h as well.
enum class AppType {
  NON_APP = 0,
  BROWSER,
  CHROME_APP,
  ARC_APP,
  CROSTINI_APP,
  SYSTEM_APP,
  // TODO(crbug.com/40133859): Migrate this into BROWSER.
  LACROS,

  kMaxValue = LACROS,
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_APP_TYPES_H_
