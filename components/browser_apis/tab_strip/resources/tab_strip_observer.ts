// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {OnCollectionCreatedEvent, OnDataChangedEvent, OnNodeMovedEvent, OnTabsClosedEvent, OnTabsCreatedEvent} from './tab_strip_api_events.mojom-webui.js';

// Interface based tab strip observer. Clients are strongly recommended to
// implement the complete interface and to avoid the partial<> modifier,
// because that would make it difficult for the API maintainers to update
// clients for future API updates.
export interface TabStripObserver {
  onTabsCreated: (event: OnTabsCreatedEvent) => void;
  onTabsClosed: (event: OnTabsClosedEvent) => void;
  onDataChanged: (event: OnDataChangedEvent) => void;
  onCollectionCreated: (event: OnCollectionCreatedEvent) => void;
  onNodeMoved: (event: OnNodeMovedEvent) => void;
}
