// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/fake_tab_id_generator.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/session_id.h"

int32_t FakeTabIdGenerator::GenerateTabHandleId(
    const tabs::TabInterface* tab) const {
  return static_cast<int32_t>(
      base::PersistentHash(base::UTF16ToUTF8(tab->GetContents()->GetTitle())));
}

const FakeTabIdGenerator* FakeTabIdGenerator::Get() {
  static const base::NoDestructor<FakeTabIdGenerator> kInstance;
  return kInstance.get();
}
