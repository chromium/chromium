// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_FAKE_ELEMENT_STORE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_FAKE_ELEMENT_STORE_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_store.h"

namespace autofill_assistant {

class FakeElementStore : public ElementStore {
 public:
  FakeElementStore();
  FakeElementStore(content::WebContents* web_contents);
  ~FakeElementStore() override;

  FakeElementStore(const FakeElementStore&) = delete;
  FakeElementStore& operator=(const FakeElementStore&) = delete;

  ClientStatus GetElement(const std::string& client_id,
                          ElementFinderResult* out_element) const override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_FAKE_ELEMENT_STORE_H_
