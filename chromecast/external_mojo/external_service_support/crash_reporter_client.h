// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_CRASH_REPORTER_CLIENT_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_CRASH_REPORTER_CLIENT_H_

namespace chromecast {
namespace external_service_support {

class CrashReporterClient {
 public:
  static void Init();
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_CRASH_REPORTER_CLIENT_H_
