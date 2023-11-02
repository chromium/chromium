// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_
#define CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_

namespace sharing {

// Logs number of ice servers fetched from network traversal api call.
void LogWebRtcIceConfigFetched(int count);

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_PUBLIC_CPP_SHARING_WEBRTC_METRICS_H_
