// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/test/mock_quick_answers_client.h"

namespace quick_answers {

MockQuickAnswersClient::MockQuickAnswersClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    QuickAnswersDelegate* quick_answers_delegate)
    : QuickAnswersClient(url_loader_factory, quick_answers_delegate) {}

MockQuickAnswersClient::~MockQuickAnswersClient() {}

}  // namespace quick_answers
