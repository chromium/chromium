// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for MessageQueueManagerImpl.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageQueueManagerImplTest {
    @Test
    public void testCreateMessageQueueManager() {
        MessageQueueManagerImpl queueManager = new MessageQueueManagerImpl();
    }
}
