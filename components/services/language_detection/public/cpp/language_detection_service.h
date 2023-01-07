// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_LANGUAGE_DETECTION_PUBLIC_CPP_LANGUAGE_DETECTION_SERVICE_H_
#define COMPONENTS_SERVICES_LANGUAGE_DETECTION_PUBLIC_CPP_LANGUAGE_DETECTION_SERVICE_H_

#include "components/services/language_detection/public/mojom/language_detection.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace language_detection {

// Launches a new instance of the LanguageDetectionService in an isolated,
// sandboxed process, and returns a remote interface to control the service. The
// lifetime of the process is tied to that of the Remote. May be called from any
// thread.
mojo::Remote<mojom::LanguageDetectionService> LaunchLanguageDetectionService();

}  // namespace language_detection

#endif  // COMPONENTS_SERVICES_LANGUAGE_DETECTION_PUBLIC_CPP_LANGUAGE_DETECTION_SERVICE_H_
