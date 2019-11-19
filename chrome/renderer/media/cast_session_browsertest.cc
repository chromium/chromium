// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session.h"

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

typedef ChromeRenderViewTest CastSessionBrowserTest;

// Tests that CastSession is created and destroyed properly inside
// chrome renderer.
TEST_F(CastSessionBrowserTest, CreateAndDestroy) {
  chrome_render_thread_->set_io_task_runner(
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  ChromeContentRendererClient* client =
      static_cast<ChromeContentRendererClient*>(content_renderer_client_.get());
  client->RenderThreadStarted();

  scoped_refptr<CastSession> session(
      new CastSession(blink::scheduler::GetSingleThreadTaskRunnerForTesting()));

  // Causes CastSession to destruct.
  session.reset();
  base::RunLoop().RunUntilIdle();
}
