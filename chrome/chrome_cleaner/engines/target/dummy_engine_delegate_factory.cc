// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_delegate_factory.h"

#include "chrome/chrome_cleaner/engines/target/test_engine_delegate.h"

namespace chrome_cleaner {

scoped_refptr<EngineDelegate> CreateEngineDelegate(Engine::Name engine) {
  CHECK_EQ(engine, Engine::TEST_ONLY);
  return base::MakeRefCounted<chrome_cleaner::TestEngineDelegate>();
}

}  // namespace chrome_cleaner
