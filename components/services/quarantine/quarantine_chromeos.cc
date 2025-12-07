// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "url/gurl.h"

namespace quarantine {

void OnFileAdded(mojom::Quarantine::QuarantineFileCallback callback,
                 const dlp::AddFilesResponse response) {
  if (response.has_error_message()) {
    DVLOG(1) << "Failed to quarantine: " << response.error_message();
    std::move(callback).Run(QuarantineFileResult::ANNOTATION_FAILED);
    return;
  }
  std::move(callback).Run(QuarantineFileResult::OK);
}

void QuarantineFile(const base::FilePath& file,
                    const GURL& source_url_unsafe,
                    const GURL& referrer_url_unsafe,
                    const std::optional<url::Origin>& request_initiator,
                    const std::string& client_guid,
                    mojom::Quarantine::QuarantineFileCallback callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(callback).Run(QuarantineFileResult::OK);
    return;
  }

  ::dlp::AddFilesRequest request;
  ::dlp::AddFileRequest* add_request = request.add_add_file_requests();
  add_request->set_file_path(file.value());
  add_request->set_source_url(source_url_unsafe.spec());
  add_request->set_referrer_url(referrer_url_unsafe.spec());
  chromeos::DlpClient::Get()->AddFiles(
      request, base::BindOnce(&OnFileAdded, std::move(callback)));
}

}  // namespace quarantine
