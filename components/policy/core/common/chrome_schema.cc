// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/chrome_schema.h"

#include "base/no_destructor.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"

namespace policy {

const Schema& GetChromeSchema() {
  static const base::NoDestructor<Schema> chrome_schema_(
      Schema::Wrap(GetChromeSchemaData()));
  return *chrome_schema_;
}

}  // namespace policy
