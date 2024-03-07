// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_METRICS_H_

namespace media_preview_metrics {

void RecordPageInfoCameraNumInUseDevices(int devices);
void RecordPageInfoMicNumInUseDevices(int devices);

}  // namespace media_preview_metrics

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_METRICS_H_
