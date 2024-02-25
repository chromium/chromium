// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.SystemClock;
import android.test.mock.MockPackageManager;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.base.PageTransition;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.Function;

/** Unittests for tab redirect handler. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class RedirectHandlerTest {
    private static final int TRANS_TYPE_OF_LINK_FROM_INTENT =
            PageTransition.LINK | PageTransition.FROM_API;
    private static final String TEST_PACKAGE_NAME = "test.package.name";
    private static Intent sYtIntent;
    private static Intent sMoblieYtIntent;
    private static Intent sFooIntent;

    private Function<Intent, List<ResolveInfo>> mQueryIntentFunction =
            (Intent intent) -> queryIntentActivities(intent);

    private Context mContextToRestore;

    static {
        try {
            sYtIntent = Intent.parseUri("http://youtube.com/", Intent.URI_INTENT_SCHEME);
            sMoblieYtIntent = Intent.parseUri("http://m.youtube.com/", Intent.URI_INTENT_SCHEME);
            sFooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException ue) {
            // Ignore exception.
        }
    }

    @Before
    public void setUp() {
        mContextToRestore = ContextUtils.getApplicationContext();
        ContextUtils.initApplicationContextForTests(new TestContext());
    }

    private List<ResolveInfo> queryIntentActivities(Intent intent) {
        return PackageManagerUtils.queryIntentActivities(intent, 0);
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testRealIntentRedirect() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0, false, false);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, true, false, 0, 0, false, false);
        Assert.assertTrue(handler.isOnNoninitialLoadForIntentNavigationChain());
        Assert.assertFalse(
                handler.hasNewResolver(
                        queryIntentActivities(sMoblieYtIntent), mQueryIntentFunction));
        Assert.assertTrue(
                handler.hasNewResolver(queryIntentActivities(sFooIntent), mQueryIntentFunction));
        Assert.assertFalse(
                handler.hasNewResolver(new ArrayList<ResolveInfo>(), mQueryIntentFunction));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testEffectiveIntentRedirect_linkNavigation() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0, false, false);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1, false, true);
        Assert.assertTrue(handler.isOnNoninitialLoadForIntentNavigationChain());
        Assert.assertFalse(
                handler.hasNewResolver(
                        queryIntentActivities(sMoblieYtIntent), mQueryIntentFunction));
        Assert.assertTrue(
                handler.hasNewResolver(queryIntentActivities(sFooIntent), mQueryIntentFunction));
        Assert.assertFalse(
                handler.hasNewResolver(new ArrayList<ResolveInfo>(), mQueryIntentFunction));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testEffectiveIntentRedirect_formSubmit() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0, false, false);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        handler.updateNewUrlLoading(PageTransition.FORM_SUBMIT, false, false, 0, 1, false, true);
        Assert.assertTrue(handler.isOnNoninitialLoadForIntentNavigationChain());
        Assert.assertFalse(
                handler.hasNewResolver(
                        queryIntentActivities(sMoblieYtIntent), mQueryIntentFunction));
        Assert.assertTrue(
                handler.hasNewResolver(queryIntentActivities(sFooIntent), mQueryIntentFunction));
        Assert.assertFalse(
                handler.hasNewResolver(new ArrayList<ResolveInfo>(), mQueryIntentFunction));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testNoIntent() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(null, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0, false, false);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, true, false, 0, 0, false, false);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        Assert.assertTrue(
                handler.hasNewResolver(
                        queryIntentActivities(sMoblieYtIntent), mQueryIntentFunction));
        Assert.assertTrue(
                handler.hasNewResolver(queryIntentActivities(sFooIntent), mQueryIntentFunction));
        Assert.assertFalse(
                handler.hasNewResolver(new ArrayList<ResolveInfo>(), mQueryIntentFunction));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testClear() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0, false, false);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, true, false, 0, 0, false, false);
        Assert.assertTrue(handler.isOnNoninitialLoadForIntentNavigationChain());
        Assert.assertFalse(
                handler.hasNewResolver(
                        queryIntentActivities(sMoblieYtIntent), mQueryIntentFunction));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        handler.clear();
        Assert.assertFalse(handler.isOnNavigation());
        Assert.assertTrue(
                handler.hasNewResolver(
                        queryIntentActivities(sMoblieYtIntent), mQueryIntentFunction));
        Assert.assertTrue(
                handler.hasNewResolver(queryIntentActivities(sFooIntent), mQueryIntentFunction));
        Assert.assertFalse(
                handler.hasNewResolver(new ArrayList<ResolveInfo>(), mQueryIntentFunction));
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testNonLinkFromIntent() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0, false, false);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1, false, true);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        Assert.assertTrue(
                handler.hasNewResolver(
                        queryIntentActivities(sMoblieYtIntent), mQueryIntentFunction));
        Assert.assertTrue(
                handler.hasNewResolver(queryIntentActivities(sFooIntent), mQueryIntentFunction));
        Assert.assertFalse(
                handler.hasNewResolver(new ArrayList<ResolveInfo>(), mQueryIntentFunction));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testUserInteraction() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0, false, false);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, true, false, 0, 0, false, false);
        Assert.assertTrue(handler.isOnNoninitialLoadForIntentNavigationChain());
        Assert.assertFalse(
                handler.hasNewResolver(
                        queryIntentActivities(sMoblieYtIntent), mQueryIntentFunction));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        handler.updateNewUrlLoading(
                PageTransition.LINK,
                false,
                true,
                SystemClock.elapsedRealtime() + 1,
                1,
                false,
                true);
        Assert.assertFalse(handler.isOnNoninitialLoadForIntentNavigationChain());
        Assert.assertTrue(
                handler.hasNewResolver(
                        queryIntentActivities(sMoblieYtIntent), mQueryIntentFunction));
        Assert.assertTrue(
                handler.hasNewResolver(queryIntentActivities(sFooIntent), mQueryIntentFunction));
        Assert.assertFalse(
                handler.hasNewResolver(new ArrayList<ResolveInfo>(), mQueryIntentFunction));

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(1, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testNavigationFromUserTyping() {
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);
        Assert.assertFalse(handler.isOnNavigation());

        handler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0, false, false);
        Assert.assertTrue(handler.isNavigationFromUserTyping());
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1, false, true);
        Assert.assertTrue(handler.isNavigationFromUserTyping());

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 2, false, true);
        Assert.assertFalse(handler.isNavigationFromUserTyping());

        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testRedirectFromCurrentNavigationShouldNotOverrideUrlLoading() {
        /////////////////////////////////////////////////////
        // 1. 3XX redirection should not override URL loading.
        /////////////////////////////////////////////////////
        RedirectHandler handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);

        handler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0, false, true);
        Assert.assertFalse(handler.shouldNotOverrideUrlLoading());
        handler.setShouldNotOverrideUrlLoadingOnCurrentRedirectChain();

        handler.updateNewUrlLoading(PageTransition.LINK, true, false, 0, 0, false, true);
        Assert.assertTrue(handler.shouldNotOverrideUrlLoading());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        /////////////////////////////////////////////////////
        // 2. Effective redirection should not override URL loading.
        /////////////////////////////////////////////////////
        handler = RedirectHandler.create();
        handler.updateIntent(sYtIntent, false, false, false);

        handler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0, false, true);
        Assert.assertFalse(handler.shouldNotOverrideUrlLoading());
        handler.setShouldNotOverrideUrlLoadingOnCurrentRedirectChain();

        // Effective redirection occurred.
        handler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1, false, true);
        Assert.assertTrue(handler.shouldNotOverrideUrlLoading());
        Assert.assertEquals(0, handler.getLastCommittedEntryIndexBeforeStartingNavigation());

        /////////////////////////////////////////////////////
        // 3. New URL loading should not be affected.
        /////////////////////////////////////////////////////
        SystemClock.sleep(1);
        handler.updateNewUrlLoading(
                PageTransition.LINK, false, true, SystemClock.elapsedRealtime(), 2, false, true);
        Assert.assertFalse(handler.shouldNotOverrideUrlLoading());
        Assert.assertEquals(2, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testNavigationWithUninitializedUserInteractionTime() {
        // User interaction time could be uninitialized when a new document activity is opened after
        // clicking a link. In that case, the value is 0.
        final long uninitializedUserInteractionTime = 0;
        RedirectHandler handler = RedirectHandler.create();

        Assert.assertFalse(handler.isOnNavigation());
        handler.updateNewUrlLoading(
                PageTransition.LINK,
                false,
                true,
                uninitializedUserInteractionTime,
                RedirectHandler.NO_COMMITTED_ENTRY_INDEX,
                /* isInitialNavigation= */ true,
                true);
        Assert.assertTrue(handler.isOnNavigation());
        Assert.assertEquals(
                RedirectHandler.NO_COMMITTED_ENTRY_INDEX,
                handler.getLastCommittedEntryIndexBeforeStartingNavigation());
        Assert.assertFalse(handler.hasUserStartedNonInitialNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testLastCommittedIndexPersistsThroughClear() {
        int lastIndex = 1234;
        RedirectHandler handler = RedirectHandler.create();
        handler.updateNewUrlLoading(
                PageTransition.LINK,
                /* isRedirect= */ false,
                /* hasUserGesture= */ false,
                0,
                lastIndex,
                /* isInitialNavigation= */ true,
                /* isRendererInitiated= */ true);
        handler.clear();
        Assert.assertEquals(
                lastIndex, handler.getLastCommittedEntryIndexBeforeStartingNavigation());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testNavigationChainExpired() {
        long navigationId = 1234;
        AtomicLong currentTime = new AtomicLong(0);
        RedirectHandler handler =
                new RedirectHandler() {
                    @Override
                    public long currentRealtime() {
                        return currentTime.get();
                    }
                };
        handler.updateNewUrlLoading(
                PageTransition.LINK,
                /* isRedirect= */ false,
                /* hasUserGesture= */ true,
                0,
                0,
                /* isInitialNavigation= */ true,
                /* isRendererInitiated= */ true);
        currentTime.set(RedirectHandler.NAVIGATION_CHAIN_TIMEOUT_MILLIS + 1);
        Assert.assertTrue(handler.isNavigationChainExpired());
    }

    @Test
    @SmallTest
    @Feature({"IntentHandling"})
    public void testCctPrefetch() {
        RedirectHandler handler = RedirectHandler.create();
        handler.setIsPrefetchLoadForIntent(true);
        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0, false, false);
        Assert.assertTrue(handler.getInitialNavigationState().isFromIntent);
        handler.clear();

        handler.updateNewUrlLoading(
                TRANS_TYPE_OF_LINK_FROM_INTENT, false, false, 0, 0, false, false);
        Assert.assertFalse(handler.getInitialNavigationState().isFromIntent);
    }

    private static class TestPackageManager extends MockPackageManager {
        @Override
        public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
            List<ResolveInfo> resolves = new ArrayList<ResolveInfo>();
            if (intent.getDataString().startsWith("http://m.youtube.com")
                    || intent.getDataString().startsWith("http://youtube.com")) {
                ResolveInfo youTubeApp = new ResolveInfo();
                youTubeApp.activityInfo = new ActivityInfo();
                youTubeApp.activityInfo.packageName = "youtube";
                youTubeApp.activityInfo.name = "youtube";
                resolves.add(youTubeApp);
            } else {
                ResolveInfo fooApp = new ResolveInfo();
                fooApp.activityInfo = new ActivityInfo();
                fooApp.activityInfo.packageName = "foo";
                fooApp.activityInfo.name = "foo";
                resolves.add(fooApp);
            }
            return resolves;
        }
    }

    private static class TestContext extends AdvancedMockContext {
        @Override
        public PackageManager getPackageManager() {
            return new TestPackageManager();
        }

        @Override
        public String getPackageName() {
            return TEST_PACKAGE_NAME;
        }
    }
}
