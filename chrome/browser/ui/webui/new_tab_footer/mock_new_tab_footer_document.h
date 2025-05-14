// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_MOCK_NEW_TAB_FOOTER_DOCUMENT_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_MOCK_NEW_TAB_FOOTER_DOCUMENT_H_

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockNewTabFooterDocument
    : public new_tab_footer::mojom::NewTabFooterDocument {
 public:
  MockNewTabFooterDocument();
  ~MockNewTabFooterDocument() override;

  mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>
  BindAndGetRemote();

  void FlushForTesting();

  MOCK_METHOD(void, SetNtpExtensionName, (const std::string&));
  MOCK_METHOD(void,
              SetManagementNotice,
              (new_tab_footer::mojom::ManagementNoticePtr));

  mojo::Receiver<new_tab_footer::mojom::NewTabFooterDocument> receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_MOCK_NEW_TAB_FOOTER_DOCUMENT_H_
