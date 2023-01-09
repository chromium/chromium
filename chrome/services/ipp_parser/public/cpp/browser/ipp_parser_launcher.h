// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_IPP_PARSER_PUBLIC_CPP_BROWSER_IPP_PARSER_LAUNCHER_H_
#define CHROME_SERVICES_IPP_PARSER_PUBLIC_CPP_BROWSER_IPP_PARSER_LAUNCHER_H_

#include "base/functional/callback.h"
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ipp_parser {

// Launches a new instance of the IppParser service in an isolated, sandboxed
// process, and returns a remote interface to control the service. The lifetime
// of the process is tied to that of the Remote. May be called from any thread.
mojo::PendingRemote<mojom::IppParser> LaunchIppParser();

}  // namespace ipp_parser

#endif  // CHROME_SERVICES_IPP_PARSER_PUBLIC_CPP_BROWSER_IPP_PARSER_LAUNCHER_H_
