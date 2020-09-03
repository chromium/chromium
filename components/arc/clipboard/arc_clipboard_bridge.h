// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_CLIPBOARD_ARC_CLIPBOARD_BRIDGE_H_
#define COMPONENTS_ARC_CLIPBOARD_ARC_CLIPBOARD_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/mojom/clipboard.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcClipboardBridge : public KeyedService,
                           public ui::ClipboardObserver,
                           public mojom::ClipboardHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcClipboardBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcClipboardBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcClipboardBridge() override;

  // ClipboardObserver overrides.
  void OnClipboardDataChanged() override;

  // mojom::ClipboardHost overrides.
  void SetClipContent(mojom::ClipDataPtr clip_data) override;
  void GetClipContent(GetClipContentCallback callback) override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  bool event_originated_at_instance_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ArcClipboardBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_CLIPBOARD_ARC_CLIPBOARD_BRIDGE_H_
