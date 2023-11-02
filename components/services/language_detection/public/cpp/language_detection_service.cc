// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/language_detection/public/cpp/language_detection_service.h"

#include "components/services/language_detection/public/mojom/language_detection.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace language_detection {

mojo::Remote<mojom::LanguageDetectionService> LaunchLanguageDetectionService() {
  return content::ServiceProcessHost::Launch<mojom::LanguageDetectionService>(
      content::ServiceProcessHost::Options()
          .WithDisplayName("Translate Language Detection")
          .Pass());
}

}  //  namespace language_detection
