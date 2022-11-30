// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/search_test_utils.h"

#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search_test_utils {

void WaitForTemplateURLServiceToLoad(TemplateURLService* service) {
  if (service->loaded())
    return;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  base::CallbackListSubscription subscription =
      service->RegisterOnLoadedCallback(
          base::BindLambdaForTesting([&]() { message_loop_runner->Quit(); }));
  service->Load();
  message_loop_runner->Run();

  ASSERT_TRUE(service->loaded());
}

}  // namespace search_test_utils
