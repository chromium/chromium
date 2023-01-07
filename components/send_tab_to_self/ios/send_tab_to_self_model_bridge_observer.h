// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_IOS_SEND_TAB_TO_SELF_MODEL_BRIDGE_OBSERVER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_IOS_SEND_TAB_TO_SELF_MODEL_BRIDGE_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

// Protocol forwarding all Send Tab To Self Model Observer methods in
// Objective-C.
@protocol SendTabToSelfModelBridgeObserver <NSObject>

@required
- (void)sendTabToSelfModelLoaded:(send_tab_to_self::SendTabToSelfModel*)model;

- (void)sendTabToSelfModel:(send_tab_to_self::SendTabToSelfModel*)model
     didAddEntriesRemotely:
         (const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&)
             new_entries;

// The Entry has already been deleted at this point and the guid cannot be used
// to access the old entry via SendTabToSelfModel::GetEntryByGUID.
- (void)sendTabToSelfModel:(send_tab_to_self::SendTabToSelfModel*)model
    didRemoveEntriesRemotely:(const std::vector<std::string>&)guids;
@end

namespace send_tab_to_self {

// Observer for the Send Tab To Self model that translates all the callbacks to
// Objective-C calls.
class SendTabToSelfModelBridge : public SendTabToSelfModelObserver {
 public:
  // It is required that |model| should be non-null. If |observer| is nil this
  // class will result in all no-ops.

  explicit SendTabToSelfModelBridge(
      id<SendTabToSelfModelBridgeObserver> observer,
      SendTabToSelfModel* model);

  SendTabToSelfModelBridge(const SendTabToSelfModelBridge&) = delete;
  SendTabToSelfModelBridge& operator=(const SendTabToSelfModelBridge&) = delete;

  ~SendTabToSelfModelBridge() override;

 private:
  void SendTabToSelfModelLoaded() override;
  void EntriesAddedRemotely(
      const std::vector<const SendTabToSelfEntry*>&) override;
  void EntriesRemovedRemotely(const std::vector<std::string>&) override;

  __weak id<SendTabToSelfModelBridgeObserver> observer_;

  SendTabToSelfModel* model_;  // weak
};

}  // namespace send_tab_to_self
#endif  // COMPONENTS_SEND_TAB_TO_SELF_IOS_SEND_TAB_TO_SELF_MODEL_BRIDGE_OBSERVER_H_
