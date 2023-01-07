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
                 const dlp::AddFileResponse response) {
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
                    const std::string& client_guid,
                    mojom::Quarantine::QuarantineFileCallback callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(callback).Run(QuarantineFileResult::OK);
    return;
  }
  dlp::AddFileRequest request;
  request.set_file_path(file.value());
  request.set_source_url(source_url_unsafe.spec());
  request.set_referrer_url(referrer_url_unsafe.spec());
  chromeos::DlpClient::Get()->AddFile(
      request, base::BindOnce(&OnFileAdded, std::move(callback)));
}

}  // namespace quarantine
