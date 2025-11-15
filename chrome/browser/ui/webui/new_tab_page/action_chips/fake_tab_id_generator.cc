// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/fake_tab_id_generator.h"

#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/session_id.h"

SessionID FakeTabIdGenerator::GenerateTabId(
    content::WebContents* contents) const {
  return SessionID::FromSerializedValue(
      std::abs(static_cast<SessionID::id_type>(
          base::PersistentHash(base::UTF16ToUTF8(contents->GetTitle())))));
}
