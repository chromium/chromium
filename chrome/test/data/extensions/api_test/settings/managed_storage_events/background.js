// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function managedChangeEvents() {
    // This test enters twice to test storage.onChanged() notifications across
    // browser restarts for the managed namespace.
    //
    // The first run is when the extension is first installed; that run has
    // some initial policy that is verified, and then settings_apitest.cc
    // changes the policy to trigger an onChanged() event.
    //
    // The second run happens after the browser has been restarted, and the
    // policy is removed. Since this test extension has lazy background pages
    // it will start "idle", and the onChanged event should wake it up.
    //
    // |was_first_run| tracks whether onInstalled ever fired, so that the
    // onChanged() listener can probe for the correct policies.
    var was_first_run = false;

    // This needs to start as true since we only check the initial policies
    // in the PRE_ step and it needs to be true during the main test. This
    // gets set to false immediately on entering the onInstalled callback.
    var initial_policies_verified = true;

    // Due to a race between setting the initial policies in the C++ side
    // and running the test, there may be an additional change event that
    // comes through. If so, we should ignore it.
    var initial_change_event = {
      'changes-policy': { 'newValue':'bbb' },
      'constant-policy': { 'newValue':'aaa' },
      'deleted-policy': { 'newValue':'ccc' }
    };

    // This only enters on PRE_ManagedStorageEvents, when the extension is
    // first installed.
    chrome.runtime.onInstalled.addListener(function() {
      initial_policies_verified = false;
      was_first_run = true;

      // Verify initial policy.
      chrome.storage.managed.get(function(results) {
        // Check to see if we get no policies and return immediately if so.
        // In that case, we'll verify the initial policies in the onChanged()
        // handler below.
        if (Object.keys(results).length == 0) {
          return;
        }

        // The policies may have been verified in the onChanged listener.
        if (initial_policies_verified) {
          return;
        }

        chrome.test.assertEq({
          'constant-policy': 'aaa',
          'changes-policy': 'bbb',
          'deleted-policy': 'ccc'
        }, results);
        initial_policies_verified = true;
        // Signal to the browser that the extension had performed the
        // initial load. The browser will change the policy and trigger
        // onChanged(). Note that this listener function is executed after
        // adding the onChanged() listener below.
        chrome.test.sendMessage('ready');
      });
    });

    // Listen for onChanged() events.
    //
    // Note: don't use chrome.test.listenOnce() here! The onChanged() listener
    // must stay in place, otherwise the extension won't receive an event after
    // restarting!
    chrome.storage.onChanged.addListener(function(changes, namespace) {
      // We should only see events for the 'managed' namespace.
      chrome.test.assertEq('managed', namespace);

      // If the initial policies weren't verified, this onChanged event
      // should contain those changes. In that case, we need to verify
      // them and send the 'ready' message.
      if (!initial_policies_verified) {
        chrome.test.assertEq(initial_change_event, changes);
        // Initial policies are now verified, so signal the browser
        // to start the test.
        initial_policies_verified = true;
        chrome.test.sendMessage('ready');
        return;
      }

      // We might also get an oncChanged event with those same changes
      // _after_ the policies have been verified by the onInstalled
      // listener. In that case, just ignore them.
      if (chrome.test.checkDeepEq(changes, initial_change_event)) {
        return;
      }

      let expectedChanges;
      if (was_first_run) {
        expectedChanges = {
          'changes-policy': {
            'oldValue': 'bbb',
            'newValue': 'ddd'
          },
          'deleted-policy': { 'oldValue': 'ccc' },
          'new-policy': { 'newValue': 'eee' }
        };
      } else {
        expectedChanges = {
          'changes-policy': { 'oldValue': 'ddd' },
          'constant-policy': { 'oldValue': 'aaa' },
          'new-policy': { 'oldValue': 'eee' }
        };
      }

      chrome.test.assertEq(expectedChanges, changes);
      chrome.test.succeed();
    });
  }
]);
