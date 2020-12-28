// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.content.Context;

import org.chromium.chrome.browser.signin.services.SigninHelper;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Fake AccountChangeEventChecker for testing.
 */
public final class MockChangeEventChecker
        implements SigninHelper.AccountChangeEventChecker {
    private final Map<String, List<String>> mEvents = new HashMap<>();

    @Override
    public List<String> getAccountChangeEvents(
            Context context, int index, String accountName) {
        List<String> eventsList = getEventList(accountName);
        return eventsList.subList(index, eventsList.size());
    }

    public void insertRenameEvent(String from, String to) {
        List<String> eventsList = getEventList(from);
        eventsList.add(to);
    }

    private List<String> getEventList(String account) {
        List<String> eventsList = mEvents.get(account);
        if (eventsList == null) {
            eventsList = new ArrayList<>();
            mEvents.put(account, eventsList);
        }
        return eventsList;
    }
}
