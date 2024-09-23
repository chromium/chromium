// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PROTO_PROTO_HELPERS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PROTO_PROTO_HELPERS_H_

#include <iosfwd>

namespace web_app::proto {
enum InstallState : int;

std::ostream& operator<<(std::ostream& os, const InstallState& state);

}  // namespace web_app::proto

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PROTO_PROTO_HELPERS_H_
