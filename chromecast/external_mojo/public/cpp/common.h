// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_PUBLIC_CPP_COMMON_H_
#define CHROMECAST_EXTERNAL_MOJO_PUBLIC_CPP_COMMON_H_

#include <string>

namespace chromecast {
namespace external_mojo {

// Returns the Unix domain socket path to use to connect to the Mojo broker.
// You can set the path on the command line with --mojo_broker_path=<path>.
std::string GetBrokerPath();

}  // namespace external_mojo
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_PUBLIC_CPP_COMMON_H_
