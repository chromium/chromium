// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_

#include "components/sync/driver/model_type_controller.h"

namespace autofill {

class ContactInfoModelTypeController : public syncer::ModelTypeController {
 public:
  ContactInfoModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode);

  ContactInfoModelTypeController(const ContactInfoModelTypeController&) =
      delete;
  ContactInfoModelTypeController& operator=(
      const ContactInfoModelTypeController&) = delete;

  // ModelTypeController overrides.
  bool ShouldRunInTransportOnlyMode() const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_
