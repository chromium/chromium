// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/field_data_manager_factory_ios.h"

namespace autofill {

// static
FieldDataManager* FieldDataManagerFactoryIOS::FromWebFrame(
    web::WebFrame* frame) {
  CHECK(frame);

  // No-op if it exists already.
  FieldDataManagerHolderIOS::CreateForWebFrame(frame);

  return FieldDataManagerHolderIOS::FromWebFrame(frame)->get();
}

// static
const scoped_refptr<FieldDataManager> FieldDataManagerFactoryIOS::GetRetainable(
    web::WebFrame* frame) {
  CHECK(frame);

  // No-op if it exists already.
  FieldDataManagerHolderIOS::CreateForWebFrame(frame);

  auto fdm = FieldDataManagerHolderIOS::FromWebFrame(frame)->GetRetainable();
  return fdm;
}

FieldDataManagerHolderIOS::FieldDataManagerHolderIOS(web::WebFrame* frame)
    : manager_(base::MakeRefCounted<FieldDataManager>()) {}

FieldDataManagerHolderIOS::~FieldDataManagerHolderIOS() = default;

}  // namespace autofill
