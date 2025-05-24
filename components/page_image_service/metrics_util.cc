// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/metrics_util.h"

namespace page_image_service {

std::string ClientIdToString(mojom::ClientId client_id) {
  switch (client_id) {
    case mojom::ClientId::Journeys:
      return "Journeys";
    case mojom::ClientId::JourneysSidePanel:
      return "JourneysSidePanel";
    case mojom::ClientId::NtpRealbox:
      return "NtpRealbox";
    case mojom::ClientId::NtpQuests:
      return "NtpQuests";
    case mojom::ClientId::Bookmarks:
      return "Bookmarks";
    case mojom::ClientId::NtpTabResumption:
      return "NtpTabResumption";
    case mojom::ClientId::HistoryEmbeddings:
      return "HistoryEmbeddings";
      // No default case and no final statement, so that the compiler will force
      // developers to update this function if the ClientId enum is updated.
      // Developers must ALSO update the PageImageServiceClientId variants in
      // others/histograms.xml.
  }
}

}  // namespace page_image_service
