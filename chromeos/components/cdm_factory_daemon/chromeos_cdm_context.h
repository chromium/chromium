// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CHROMEOS_CDM_CONTEXT_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CHROMEOS_CDM_CONTEXT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"

namespace media {
class DecryptConfig;
}

namespace chromeos {

// Interface for ChromeOS CDM Factory Daemon specific extensions to the
// CdmContext interface.
class ChromeOsCdmContext {
 public:
  ChromeOsCdmContext() = default;

  using GetHwKeyDataCB =
      base::OnceCallback<void(media::Decryptor::Status status,
                              const std::vector<uint8_t>& key_data)>;

  // Gets the HW specific key information for the key specified in
  // |decrypt_config| and returns it via |callback|.
  virtual void GetHwKeyData(const media::DecryptConfig* decrypt_config,
                            const std::vector<uint8_t>& hw_identifier,
                            GetHwKeyDataCB callback) = 0;

  // Gets a CdmContextRef linked with the associated CDM for keeping it alive.
  virtual std::unique_ptr<media::CdmContextRef> GetCdmContextRef() = 0;

  // Returns true if this is coming from a CDM in ARC.
  virtual bool UsingArcCdm() const = 0;

 protected:
  virtual ~ChromeOsCdmContext() = default;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CHROMEOS_CDM_CONTEXT_H_