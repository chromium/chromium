// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/mojo/binder_factory.h"

namespace chromecast {

MultiBinderFactory::MultiBinderFactory() = default;

MultiBinderFactory::~MultiBinderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace chromecast
