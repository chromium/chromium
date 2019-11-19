// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.invalidation;

import android.accounts.Account;
import android.content.ComponentName;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.test.ServiceTestCase;

import com.google.ipc.invalidation.external.client.InvalidationListener.RegistrationState;
import com.google.ipc.invalidation.external.client.contrib.AndroidListener;
import com.google.ipc.invalidation.external.client.types.ErrorInfo;
import com.google.ipc.invalidation.external.client.types.Invalidation;
import com.google.ipc.invalidation.external.client.types.ObjectId;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CollectionUtil;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.ModelTypeHelper;
import org.chromium.components.sync.notifier.InvalidationIntentProtocol;
import org.chromium.components.sync.notifier.InvalidationPreferences;
import org.chromium.components.sync.notifier.InvalidationPreferences.EditContext;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Tests for the {@link InvalidationClientService}.
 *
 * @author dsmyers@google.com (Daniel Myers)
 *
 * TODO(yolandyan): refactor this by property bind and wait for the intent service call
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class InvalidationClientServiceTest extends
          ServiceTestCase<TestableInvalidationClientService> {

    /** Id used when creating clients. */
    private static final byte[] CLIENT_ID = new byte[]{0, 4, 7};

    /** Intents provided to {@link #startService}. */
    private List<Intent> mStartServiceIntents;

    public InvalidationClientServiceTest() {
        super(TestableInvalidationClientService.class);
    }

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        mStartServiceIntents = new ArrayList<>();
        setContext(new AdvancedMockContext(InstrumentationRegistry.getTargetContext()) {
            @Override
            public ComponentName startService(Intent intent) {
                mStartServiceIntents.add(intent);
                return new ComponentName(this, InvalidationClientServiceTest.class);
            }
        });
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        setupService();
    }

    @Override
    @After
    public void tearDown() throws Exception {
        if (InvalidationClientService.getIsClientStartedForTest()) {
            Intent stopIntent = createStopIntent();
            getService().onHandleIntent(stopIntent);
        }
        Assert.assertFalse(InvalidationClientService.getIsClientStartedForTest());
        super.tearDown();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testComputeRegistrationOps() {
        /*
         * Test plan: compute the set of registration operations resulting from various combinations
         * of existing and desired registrations. Verifying that they are correct.
         */
        Set<ObjectId> regAccumulator = new HashSet<>();
        Set<ObjectId> unregAccumulator = new HashSet<>();

        // Empty existing and desired registrations should yield empty operation sets.
        InvalidationClientService.computeRegistrationOps(
                toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS),
                toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS),
                regAccumulator, unregAccumulator);
        Assert.assertEquals(0, regAccumulator.size());
        Assert.assertEquals(0, unregAccumulator.size());

        // Equal existing and desired registrations should yield empty operation sets.
        InvalidationClientService.computeRegistrationOps(new HashSet<ObjectId>(),
                new HashSet<ObjectId>(), regAccumulator, unregAccumulator);
        Assert.assertEquals(0, regAccumulator.size());
        Assert.assertEquals(0, unregAccumulator.size());

        // Empty existing and non-empty desired registrations should yield desired registrations
        // as the registration operations to do and no unregistrations.
        Set<ObjectId> desiredTypes = toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS);
        InvalidationClientService.computeRegistrationOps(
                new HashSet<ObjectId>(),
                desiredTypes,
                regAccumulator, unregAccumulator);
        Assert.assertEquals(
                toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS),
                new HashSet<>(regAccumulator));
        Assert.assertEquals(0, unregAccumulator.size());
        regAccumulator.clear();

        // Unequal existing and desired registrations should yield both registrations and
        // unregistrations. We should unregister TYPED_URLS and register BOOKMARKS, keeping
        // SESSIONS.
        InvalidationClientService.computeRegistrationOps(
                toObjectIdSet(ModelType.SESSIONS, ModelType.TYPED_URLS),
                toObjectIdSet(ModelType.BOOKMARKS, ModelType.SESSIONS),
                regAccumulator, unregAccumulator);
        Assert.assertEquals(toObjectIdSet(ModelType.BOOKMARKS), regAccumulator);
        Assert.assertEquals(toObjectIdSet(ModelType.TYPED_URLS), unregAccumulator);
        regAccumulator.clear();
        unregAccumulator.clear();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testReady() {
       /**
        * Test plan: call ready. Verify that the service sets the client id correctly and reissues
        * pending registrations.
        */

        // Persist some registrations.
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        EditContext editContext = invPrefs.edit();
        invPrefs.setSyncTypes(editContext, CollectionUtil.newArrayList("BOOKMARK", "SESSION"));
        ObjectId objectId = ObjectId.newInstance(1, ApiCompatibilityUtils.getBytesUtf8("obj"));
        invPrefs.setObjectIds(editContext, CollectionUtil.newArrayList(objectId));
        Assert.assertTrue(invPrefs.commit(editContext));

        // Issue ready.
        getService().ready(CLIENT_ID);
        Assert.assertTrue(Arrays.equals(CLIENT_ID, InvalidationClientService.getClientIdForTest()));
        byte[] otherCid = ApiCompatibilityUtils.getBytesUtf8("otherCid");
        getService().ready(otherCid);
        Assert.assertTrue(Arrays.equals(otherCid, InvalidationClientService.getClientIdForTest()));

        // Verify registrations issued.
        Assert.assertEquals(CollectionUtil.newHashSet(
                toObjectId(ModelType.BOOKMARKS), toObjectId(ModelType.SESSIONS), objectId),
                new HashSet<>(getService().mRegistrations.get(0)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testReissueRegistrations() {
        /*
         * Test plan: call the reissueRegistrations method of the listener with both empty and
         * non-empty sets of desired registrations stored in preferences. Verify that no register
         * intent is set in the first case and that the appropriate register intent is sent in
         * the second.
         */

        // No persisted registrations.
        getService().reissueRegistrations(CLIENT_ID);
        Assert.assertTrue(getService().mRegistrations.isEmpty());

        // Persist some registrations.
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        EditContext editContext = invPrefs.edit();
        invPrefs.setSyncTypes(editContext, CollectionUtil.newArrayList("BOOKMARK", "SESSION"));
        ObjectId objectId = ObjectId.newInstance(1, ApiCompatibilityUtils.getBytesUtf8("obj"));
        invPrefs.setObjectIds(editContext, CollectionUtil.newArrayList(objectId));
        Assert.assertTrue(invPrefs.commit(editContext));

        // Reissue registrations and verify that the appropriate registrations are issued.
        getService().reissueRegistrations(CLIENT_ID);
        Assert.assertEquals(1, getService().mRegistrations.size());
        Assert.assertEquals(CollectionUtil.newHashSet(
                toObjectId(ModelType.BOOKMARKS), toObjectId(ModelType.SESSIONS), objectId),
                new HashSet<>(getService().mRegistrations.get(0)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testInformRegistrationStatus() {
        /*
         * Test plan: call inform registration status under a variety of circumstances and verify
         * that the appropriate (un)register calls are issued.
         *
         * 1. Registration of desired object. No calls issued.
         * 2. Unregistration of undesired object. No calls issued.
         * 3. Registration of undesired object. Unregistration issued.
         * 4. Unregistration of desired object. Registration issued.
         */
        // Initial test setup: persist a single registration into preferences.
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        EditContext editContext = invPrefs.edit();
        invPrefs.setSyncTypes(editContext, CollectionUtil.newArrayList("SESSION"));
        ObjectId desiredObjectId =
                ObjectId.newInstance(1, ApiCompatibilityUtils.getBytesUtf8("obj1"));
        ObjectId undesiredObjectId =
                ObjectId.newInstance(1, ApiCompatibilityUtils.getBytesUtf8("obj2"));
        invPrefs.setObjectIds(editContext, CollectionUtil.newArrayList(desiredObjectId));
        Assert.assertTrue(invPrefs.commit(editContext));

        // Cases 1 and 2: calls matching desired state cause no actions.
        getService().informRegistrationStatus(CLIENT_ID, toObjectId(ModelType.SESSIONS),
                RegistrationState.REGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, desiredObjectId,
                RegistrationState.REGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, toObjectId(ModelType.BOOKMARKS),
                RegistrationState.UNREGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, undesiredObjectId,
                RegistrationState.UNREGISTERED);
        Assert.assertTrue(getService().mRegistrations.isEmpty());
        Assert.assertTrue(getService().mUnregistrations.isEmpty());

        // Case 3: registration of undesired object triggers an unregistration.
        getService().informRegistrationStatus(CLIENT_ID, toObjectId(ModelType.BOOKMARKS),
                RegistrationState.REGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, undesiredObjectId,
                RegistrationState.REGISTERED);
        Assert.assertEquals(2, getService().mUnregistrations.size());
        Assert.assertEquals(0, getService().mRegistrations.size());
        Assert.assertEquals(CollectionUtil.newArrayList(toObjectId(ModelType.BOOKMARKS)),
                getService().mUnregistrations.get(0));
        Assert.assertEquals(CollectionUtil.newArrayList(undesiredObjectId),
                getService().mUnregistrations.get(1));

        // Case 4: unregistration of a desired object triggers a registration.
        getService().informRegistrationStatus(CLIENT_ID, toObjectId(ModelType.SESSIONS),
                RegistrationState.UNREGISTERED);
        getService().informRegistrationStatus(CLIENT_ID, desiredObjectId,
                RegistrationState.UNREGISTERED);
        Assert.assertEquals(2, getService().mUnregistrations.size());
        Assert.assertEquals(2, getService().mRegistrations.size());
        Assert.assertEquals(CollectionUtil.newArrayList(toObjectId(ModelType.SESSIONS)),
                getService().mRegistrations.get(0));
        Assert.assertEquals(CollectionUtil.newArrayList(desiredObjectId),
                getService().mRegistrations.get(1));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testInformRegistrationFailure() {
        /*
         * Test plan: call inform registration failure under a variety of circumstances and verify
         * that the appropriate (un)register calls are issued.
         *
         * 1. Transient registration failure for an object that should be registered. Register
         *    should be called.
         * 2. Permanent registration failure for an object that should be registered. No calls.
         * 3. Transient registration failure for an object that should not be registered. Unregister
         *    should be called.
         * 4. Permanent registration failure for an object should not be registered. No calls.
         */

        // Initial test setup: persist a single registration into preferences.
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        EditContext editContext = invPrefs.edit();
        invPrefs.setSyncTypes(editContext, CollectionUtil.newArrayList("SESSION"));
        ObjectId desiredObjectId =
                ObjectId.newInstance(1, ApiCompatibilityUtils.getBytesUtf8("obj1"));
        ObjectId undesiredObjectId =
                ObjectId.newInstance(1, ApiCompatibilityUtils.getBytesUtf8("obj2"));
        invPrefs.setObjectIds(editContext, CollectionUtil.newArrayList(desiredObjectId));
        Assert.assertTrue(invPrefs.commit(editContext));

        // Cases 2 and 4: permanent registration failures never cause calls to be made.
        getService().informRegistrationFailure(CLIENT_ID, toObjectId(ModelType.SESSIONS), false,
                "");
        getService().informRegistrationFailure(CLIENT_ID, toObjectId(ModelType.BOOKMARKS), false,
                "");
        getService().informRegistrationFailure(CLIENT_ID, desiredObjectId, false, "");
        getService().informRegistrationFailure(CLIENT_ID, undesiredObjectId, false, "");
        Assert.assertTrue(getService().mRegistrations.isEmpty());
        Assert.assertTrue(getService().mUnregistrations.isEmpty());

        // Case 1: transient failure of a desired registration results in re-registration.
        getService().informRegistrationFailure(CLIENT_ID, toObjectId(ModelType.SESSIONS), true, "");
        getService().informRegistrationFailure(CLIENT_ID, desiredObjectId, true, "");
        Assert.assertEquals(2, getService().mRegistrations.size());
        Assert.assertTrue(getService().mUnregistrations.isEmpty());
        Assert.assertEquals(CollectionUtil.newArrayList(toObjectId(ModelType.SESSIONS)),
                getService().mRegistrations.get(0));
        Assert.assertEquals(CollectionUtil.newArrayList(desiredObjectId),
                getService().mRegistrations.get(1));

        // Case 3: transient failure of an undesired registration results in unregistration.
        getService().informRegistrationFailure(CLIENT_ID, toObjectId(ModelType.BOOKMARKS), true,
                "");
        getService().informRegistrationFailure(CLIENT_ID, undesiredObjectId, true, "");
        Assert.assertEquals(2, getService().mRegistrations.size());
        Assert.assertEquals(2, getService().mUnregistrations.size());
        Assert.assertEquals(CollectionUtil.newArrayList(toObjectId(ModelType.BOOKMARKS)),
                getService().mUnregistrations.get(0));
        Assert.assertEquals(CollectionUtil.newArrayList(undesiredObjectId),
                getService().mUnregistrations.get(1));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testInformError() {
        /*
         * Test plan: call informError with both permanent and transient errors. Verify that
         * the transient error causes no action to be taken and that the permanent error causes
         * the client to be stopped.
         */

        // Client needs to be started for the permament error to trigger and stop.
        getService().setShouldRunStates(true, true);
        getService().onCreate();
        getService().onHandleIntent(createStartIntent());
        getService().mStartedServices.clear();  // Discard start intent.

        // Transient error.
        getService().informError(ErrorInfo.newInstance(0, true, "transient", null));
        Assert.assertTrue(getService().mStartedServices.isEmpty());

        // Permanent error.
        getService().informError(ErrorInfo.newInstance(0, false, "permanent", null));
        Assert.assertEquals(1, getService().mStartedServices.size());
        Intent sentIntent = getService().mStartedServices.get(0);
        Intent stopIntent = AndroidListener.createStopIntent(getContext());
        Assert.assertTrue(stopIntent.filterEquals(sentIntent));
        Assert.assertEquals(stopIntent.getExtras().keySet(), sentIntent.getExtras().keySet());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testReadWriteState() {
        /*
         * Test plan: read, write, and read the internal notification client persistent state.
         * Verify appropriate return values.
         */
        Assert.assertNull(getService().readState());
        byte[] writtenState = new byte[]{7, 4, 0};
        getService().writeState(writtenState);
        Assert.assertTrue(Arrays.equals(writtenState, getService().readState()));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testInvalidateWithPayload() {
        doTestInvalidate(true);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testInvalidateWithoutPayload() {
        doTestInvalidate(false);
    }

    private void doTestInvalidate(boolean hasPayload) {
        /*
         * Test plan: call invalidate() with an invalidation that may or may not have a payload.
         * Verify the produced bundle has the correct fields.
         */
        // Call invalidate.
        int version = 4747;
        int objectSource = 55;
        String objectName = "BOOKMARK";
        ObjectId objectId =
                ObjectId.newInstance(objectSource, ApiCompatibilityUtils.getBytesUtf8(objectName));
        final String payload = "testInvalidate-" + hasPayload;
        Invalidation invalidation = hasPayload
                ? Invalidation.newInstance(
                          objectId, version, ApiCompatibilityUtils.getBytesUtf8(payload))
                : Invalidation.newInstance(objectId, version);
        byte[] ackHandle = ApiCompatibilityUtils.getBytesUtf8("testInvalidate-" + hasPayload);
        getService().invalidate(invalidation, ackHandle);

        // Validate bundle.
        Assert.assertEquals(1, getService().mRequestedSyncs.size());
        PendingInvalidation request = new PendingInvalidation(getService().mRequestedSyncs.get(0));
        Assert.assertEquals(objectSource, request.mObjectSource);
        Assert.assertEquals(objectName, request.mObjectId);
        Assert.assertEquals(version, request.mVersion);
        Assert.assertEquals(hasPayload ? payload : null, request.mPayload);

        // Ensure acknowledged.
        assertSingleAcknowledgement(ackHandle);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testInvalidateUnknownVersion() {
        /*
         * Test plan: call invalidateUnknownVersion(). Verify the produced bundle has the correct
         * fields.
         */
        int objectSource = 55;
        String objectName = "BOOKMARK";
        ObjectId objectId =
                ObjectId.newInstance(objectSource, ApiCompatibilityUtils.getBytesUtf8(objectName));
        byte[] ackHandle = ApiCompatibilityUtils.getBytesUtf8("testInvalidateUV");
        getService().invalidateUnknownVersion(objectId, ackHandle);

        // Validate bundle.
        Assert.assertEquals(1, getService().mRequestedSyncs.size());
        PendingInvalidation request = new PendingInvalidation(getService().mRequestedSyncs.get(0));
        Assert.assertEquals(objectSource, request.mObjectSource);
        Assert.assertEquals(objectName, request.mObjectId);
        Assert.assertEquals(0, request.mVersion);
        Assert.assertEquals(null, request.mPayload);

        // Ensure acknowledged.
        assertSingleAcknowledgement(ackHandle);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testInvalidateAll() {
        /*
         * Test plan: call invalidateAll(). Verify the produced bundle has the correct fields.
         */
        byte[] ackHandle = ApiCompatibilityUtils.getBytesUtf8("testInvalidateAll");
        getService().invalidateAll(ackHandle);

        // Validate bundle.
        Assert.assertEquals(1, getService().mRequestedSyncs.size());
        PendingInvalidation request = new PendingInvalidation(getService().mRequestedSyncs.get(0));
        Assert.assertEquals(0, request.mObjectSource);

        // Ensure acknowledged.
        assertSingleAcknowledgement(ackHandle);
    }

    /** Asserts that the service received a single acknowledgement with handle {@code ackHandle}. */
    private void assertSingleAcknowledgement(byte[] ackHandle) {
        Assert.assertEquals(1, getService().mAcknowledgements.size());
        Assert.assertTrue(Arrays.equals(ackHandle, getService().mAcknowledgements.get(0)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testShouldClientBeRunning() {
        /*
         * Test plan: call shouldClientBeRunning with various combinations of
         * in-foreground/sync-enabled. Verify appropriate return values.
         */
        getService().setShouldRunStates(false, false);
        Assert.assertFalse(getService().shouldClientBeRunning());

        getService().setShouldRunStates(false, true);
        Assert.assertFalse(getService().shouldClientBeRunning());

        getService().setShouldRunStates(true, false);
        Assert.assertFalse(getService().shouldClientBeRunning());

        // Should only be running if both in the foreground and sync is enabled.
        getService().setShouldRunStates(true, true);
        Assert.assertTrue(getService().shouldClientBeRunning());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testStartAndStopClient() {
        /*
         * Test plan: with Chrome configured so that the client should run, send it an empty
         * intent. Even though no owning account is known, the client should still start. Send
         * it a stop intent and verify that it stops.
         */

        // Note: we are manipulating the service object directly, rather than through startService,
        // because otherwise we would need to handle the asynchronous execution model of the
        // underlying IntentService.
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        Intent startIntent = createStartIntent();
        getService().onHandleIntent(startIntent);
        Assert.assertTrue(InvalidationClientService.getIsClientStartedForTest());

        Intent stopIntent = createStopIntent();
        getService().onHandleIntent(stopIntent);
        Assert.assertFalse(InvalidationClientService.getIsClientStartedForTest());

        // The issued intents should have been an AndroidListener start intent followed by an
        // AndroidListener stop intent.
        Assert.assertEquals(2, mStartServiceIntents.size());
        Assert.assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));
        Assert.assertTrue(isAndroidListenerStopIntent(mStartServiceIntents.get(1)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testClientStopsWhenShouldNotBeRunning() {
        /*
         * Test plan: start the client. Then, change the configuration so that Chrome should not
         * be running. Send an intent to the service and verify that it stops.
         */
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Start the service.
        Intent startIntent = createStartIntent();
        getService().onHandleIntent(startIntent);
        Assert.assertTrue(InvalidationClientService.getIsClientStartedForTest());

        // Change configuration.
        getService().setShouldRunStates(false, false);

        // Send an Intent and verify that the service stops.
        getService().onHandleIntent(startIntent);
        Assert.assertFalse(InvalidationClientService.getIsClientStartedForTest());

        // The issued intents should have been an AndroidListener start intent followed by an
        // AndroidListener stop intent.
        Assert.assertEquals(2, mStartServiceIntents.size());
        Assert.assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));
        Assert.assertTrue(isAndroidListenerStopIntent(mStartServiceIntents.get(1)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntent() {
        /*
         * Test plan: send a registration-change intent. Verify that it starts the client and
         * sets both the account and registrations in shared preferences.
         */
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Send register Intent.
        Set<Integer> desiredRegistrations = CollectionUtil.newHashSet(
                ModelType.BOOKMARKS, ModelType.SESSIONS);
        Account account = AccountManagerFacade.createAccountFromName("test@example.com");
        Intent registrationIntent = createRegisterIntent(account, desiredRegistrations);
        getService().onHandleIntent(registrationIntent);

        // Verify client started and state written.
        Assert.assertTrue(InvalidationClientService.getIsClientStartedForTest());
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        Assert.assertEquals(account, invPrefs.getSavedSyncedAccount());
        Assert.assertEquals(modelTypesToNotificationTypes(desiredRegistrations),
                invPrefs.getSavedSyncedTypes());
        Assert.assertNull(invPrefs.getSavedObjectIds());
        Assert.assertEquals(1, mStartServiceIntents.size());
        Assert.assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));

        // Send a registration-change intent with different types to register for.
        desiredRegistrations = CollectionUtil.newHashSet(ModelType.PASSWORDS);
        getService().onHandleIntent(createRegisterIntent(account, desiredRegistrations));
        Assert.assertEquals(account, invPrefs.getSavedSyncedAccount());
        Assert.assertEquals(modelTypesToNotificationTypes(desiredRegistrations),
                invPrefs.getSavedSyncedTypes());
        Assert.assertEquals(1, mStartServiceIntents.size());

        // Finally, send one more registration-change intent, this time with a different account,
        // and verify that it both updates the account, stops the existing client, and
        // starts a new client.
        Account account2 = AccountManagerFacade.createAccountFromName("test2@example.com");
        getService().onHandleIntent(createRegisterIntent(account2, desiredRegistrations));
        Assert.assertEquals(account2, invPrefs.getSavedSyncedAccount());
        Assert.assertEquals(3, mStartServiceIntents.size());
        Assert.assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));
        Assert.assertTrue(isAndroidListenerStopIntent(mStartServiceIntents.get(1)));
        Assert.assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(2)));
    }

    /**
     * Determines if the correct object ids have been written to preferences and registered with the
     * invalidation client.
     *
     * @param expectedTypes The Sync types expected to be registered.
     * @param expectedObjectIds The additional object ids expected to be registered.
     * @param isReady Whether the client is ready to register/unregister.
     */
    private boolean expectedObjectIdsRegistered(Set<Integer> expectedTypes,
            Set<ObjectId> expectedObjectIds, boolean isReady) {
        // Get synced types saved to preferences.
        Set<String> expectedSyncTypes = modelTypesToNotificationTypes(expectedTypes);
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        Set<String> actualSyncTypes = invPrefs.getSavedSyncedTypes();
        if (actualSyncTypes == null) {
            actualSyncTypes = new HashSet<>();
        }

        // Get object ids saved to preferences.
        Set<ObjectId> actualObjectIds = invPrefs.getSavedObjectIds();
        if (actualObjectIds == null) {
            actualObjectIds = new HashSet<>();
        }

        // Get expected registered object ids.
        Set<ObjectId> expectedRegisteredIds = new HashSet<>();
        if (isReady) {
            expectedRegisteredIds.addAll(modelTypesToObjectIds(expectedTypes));
            expectedRegisteredIds.addAll(expectedObjectIds);
        }

        return actualSyncTypes.equals(expectedSyncTypes)
                && actualObjectIds.equals(expectedObjectIds)
                && getService().mCurrentRegistrations.equals(expectedRegisteredIds);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntentWithTypesAndObjectIds() {
        /*
         * Test plan: send a mix of registration-change intents: some for Sync types and some for
         * object ids. Verify that registering for Sync types does not interfere with object id
         * registration and vice-versa.
         */
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        Account account = AccountManagerFacade.createAccountFromName("test@example.com");
        Set<ObjectId> objectIds = new HashSet<>();
        Set<Integer> types = new HashSet<>();

        // Register for some object ids.
        objectIds.add(ObjectId.newInstance(1, ApiCompatibilityUtils.getBytesUtf8("obj1")));
        objectIds.add(ObjectId.newInstance(2, ApiCompatibilityUtils.getBytesUtf8("obj2")));
        Intent registrationIntent =
                createRegisterIntent(account, new int[] {1, 2}, new String[] {"obj1", "obj2"});
        getService().onHandleIntent(registrationIntent);
        Assert.assertTrue(expectedObjectIdsRegistered(types, objectIds, false /* isReady */));

        // Register for some types.
        types.add(ModelType.BOOKMARKS);
        types.add(ModelType.SESSIONS);
        registrationIntent = createRegisterIntent(account, types);
        getService().onHandleIntent(registrationIntent);
        Assert.assertTrue(expectedObjectIdsRegistered(types, objectIds, false /* isReady */));

        // Set client to be ready and verify registrations.
        getService().ready(CLIENT_ID);
        Assert.assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Change object id registration with types registered.
        objectIds.add(ObjectId.newInstance(3, ApiCompatibilityUtils.getBytesUtf8("obj3")));
        registrationIntent = createRegisterIntent(
            account, new int[] {1, 2, 3}, new String[] {"obj1", "obj2", "obj3"});
        getService().onHandleIntent(registrationIntent);
        Assert.assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Change type registration with object ids registered.
        types.remove(ModelType.BOOKMARKS);
        registrationIntent = createRegisterIntent(account, types);
        getService().onHandleIntent(registrationIntent);
        Assert.assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Unregister all types.
        types.clear();
        registrationIntent = createRegisterIntent(account, types);
        getService().onHandleIntent(registrationIntent);
        Assert.assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Change object id registration with no types registered.
        objectIds.remove(ObjectId.newInstance(2, ApiCompatibilityUtils.getBytesUtf8("obj2")));
        registrationIntent = createRegisterIntent(
            account, new int[] {1, 3}, new String[] {"obj1", "obj3"});
        getService().onHandleIntent(registrationIntent);
        Assert.assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Unregister all object ids.
        objectIds.clear();
        registrationIntent = createRegisterIntent(account, new int[0], new String[0]);
        getService().onHandleIntent(registrationIntent);
        Assert.assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));

        // Change type registration with no object ids registered.
        types.add(ModelType.BOOKMARKS);
        types.add(ModelType.PASSWORDS);
        registrationIntent = createRegisterIntent(account, types);
        getService().onHandleIntent(registrationIntent);
        Assert.assertTrue(expectedObjectIdsRegistered(types, objectIds, true /* isReady */));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntentNoProxyTabsUsingReady() {
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Send register Intent.
        Account account = AccountManagerFacade.createAccountFromName("test@example.com");
        Intent registrationIntent = createRegisterIntent(
                account, CollectionUtil.newHashSet(ModelType.PROXY_TABS, ModelType.SESSIONS));
        getService().onHandleIntent(registrationIntent);

        // Verify client started and state written.
        Assert.assertTrue(InvalidationClientService.getIsClientStartedForTest());
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        Assert.assertEquals(account, invPrefs.getSavedSyncedAccount());
        Assert.assertEquals(
                CollectionUtil.newHashSet("PROXY_TABS", "SESSION"), invPrefs.getSavedSyncedTypes());
        Assert.assertEquals(1, mStartServiceIntents.size());
        Assert.assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));

        // Set client to be ready. This triggers registrations.
        getService().ready(CLIENT_ID);
        Assert.assertTrue(Arrays.equals(CLIENT_ID, InvalidationClientService.getClientIdForTest()));

        // Ensure registrations are correct.
        Set<ObjectId> expectedRegistrations =
                modelTypesToObjectIds(CollectionUtil.newHashSet(ModelType.SESSIONS));
        Assert.assertEquals(expectedRegistrations,
                     new HashSet<>(getService().mRegistrations.get(0)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntentNoProxyTabsAlreadyWithClientId() {
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Send register Intent with no desired types.
        Account account = AccountManagerFacade.createAccountFromName("test@example.com");
        Intent registrationIntent = createRegisterIntent(account, new HashSet<Integer>());
        getService().onHandleIntent(registrationIntent);

        // Verify client started and state written.
        Assert.assertTrue(InvalidationClientService.getIsClientStartedForTest());
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        Assert.assertEquals(account, invPrefs.getSavedSyncedAccount());
        Assert.assertEquals(new HashSet<String>(), invPrefs.getSavedSyncedTypes());
        Assert.assertEquals(1, mStartServiceIntents.size());
        Assert.assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));

        // Make sure client is ready.
        getService().ready(CLIENT_ID);
        Assert.assertTrue(Arrays.equals(CLIENT_ID, InvalidationClientService.getClientIdForTest()));

        // Send register Intent for SESSIONS and PROXY_TABS in an already ready client.
        registrationIntent = createRegisterIntent(account,
                CollectionUtil.newHashSet(ModelType.PROXY_TABS, ModelType.SESSIONS));
        getService().onHandleIntent(registrationIntent);

        // Ensure that PROXY_TABS registration request is ignored.
        Assert.assertEquals(1, getService().mRegistrations.size());
        Set<ObjectId> expectedTypes =
                modelTypesToObjectIds(CollectionUtil.newHashSet(ModelType.SESSIONS));
        Assert.assertEquals(expectedTypes, new HashSet<>(getService().mRegistrations.get(0)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationIntentWhenClientShouldNotBeRunning() {
        /*
         * Test plan: send a registration change event when the client should not be running.
         * Verify that the service updates the on-disk state but does not start the client.
         */
        getService().onCreate();

        // Send register Intent.
        Account account = AccountManagerFacade.createAccountFromName("test@example.com");
        Set<Integer> desiredRegistrations = CollectionUtil.newHashSet(
                ModelType.BOOKMARKS, ModelType.SESSIONS);
        Intent registrationIntent = createRegisterIntent(account, desiredRegistrations);
        getService().onHandleIntent(registrationIntent);

        // Verify state written but client not started.
        Assert.assertFalse(InvalidationClientService.getIsClientStartedForTest());
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        Assert.assertEquals(account, invPrefs.getSavedSyncedAccount());
        Assert.assertEquals(modelTypesToNotificationTypes(desiredRegistrations),
                invPrefs.getSavedSyncedTypes());
        Assert.assertEquals(0, mStartServiceIntents.size());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testDeferredRegistrationsIssued() {
        /*
         * Test plan: send a registration-change intent. Verify that the client issues a start
         * intent but makes no registration calls. Issue a reissueRegistrations call and verify
         * that the client does issue the appropriate registrations.
         */
        getService().setShouldRunStates(true, true);
        getService().onCreate();

        // Send register Intent. Verify client started but no registrations issued.
        Account account = AccountManagerFacade.createAccountFromName("test@example.com");
        Set<Integer> desiredRegistrations = CollectionUtil.newHashSet(
                ModelType.BOOKMARKS, ModelType.SESSIONS);
        Set<ObjectId> desiredObjectIds = modelTypesToObjectIds(desiredRegistrations);

        Intent registrationIntent = createRegisterIntent(account, desiredRegistrations);
        getService().onHandleIntent(registrationIntent);
        Assert.assertTrue(InvalidationClientService.getIsClientStartedForTest());
        Assert.assertEquals(1, mStartServiceIntents.size());
        Assert.assertTrue(isAndroidListenerStartIntent(mStartServiceIntents.get(0)));
        InvalidationPreferences invPrefs = new InvalidationPreferences();
        Assert.assertEquals(modelTypesToNotificationTypes(desiredRegistrations),
                invPrefs.getSavedSyncedTypes());
        Assert.assertEquals(desiredObjectIds, getService().readRegistrationsFromPrefs());

        // Issue reissueRegistrations; verify registration intent issues.
        getService().reissueRegistrations(CLIENT_ID);
        Assert.assertEquals(2, mStartServiceIntents.size());
        Intent expectedRegisterIntent = AndroidListener.createRegisterIntent(
                getContext(),
                CLIENT_ID,
                desiredObjectIds);
        Intent actualRegisterIntent = mStartServiceIntents.get(1);
        Assert.assertTrue(expectedRegisterIntent.filterEquals(actualRegisterIntent));
        Assert.assertEquals(expectedRegisterIntent.getExtras().keySet(),
                actualRegisterIntent.getExtras().keySet());
        Assert.assertEquals(
                desiredObjectIds,
                new HashSet<>(getService().mRegistrations.get(0)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testNullIntent() {
        getService().setShouldRunStates(true, true);
        getService().onCreate();
        // onHandleIntent must gracefully handle receiving a null intent.
        getService().onHandleIntent(null);
        // No crash == success.
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testRegistrationRetries() {
        /*
         * Test plan: validate that the alarm receiver used by the AndroidListener underlying
         * InvalidationClientService is correctly configured in the manifest and retries
         * registrations with exponential backoff. May need to be implemented as a downstream
         * Chrome for Android test.
         */
        // TODO(dsmyers): implement.
        // Bug: https://code.google.com/p/chromium/issues/detail?id=172398
    }

    private ObjectId toObjectId(int modelType) {
        return ModelTypeHelper.toObjectId(modelType);
    }

    private Set<ObjectId> toObjectIdSet(int... modelTypes) {
        Set<ObjectId> objectIds = new HashSet<>(modelTypes.length);
        for (int i = 0; i < modelTypes.length; i++) {
            objectIds.add(toObjectId(modelTypes[i]));
        }
        return objectIds;
    }

    private Set<ObjectId> modelTypesToObjectIds(Set<Integer> modelTypes) {
        Set<ObjectId> objectIds = new HashSet<>();
        for (Integer modelType : modelTypes) {
            objectIds.add(toObjectId(modelType));
        }
        return objectIds;
    }

    private Set<String> modelTypesToNotificationTypes(Set<Integer> modelTypes) {
        Set<String> strings = new HashSet<>();
        for (Integer modelType : modelTypes) {
            strings.add(ModelTypeHelper.toNotificationType(modelType));
        }
        return strings;
    }

    /** Creates an intent to start the InvalidationClientService. */
    private Intent createStartIntent() {
        Intent intent = new Intent();
        return intent;
    }

    /** Creates an intent to stop the InvalidationClientService. */
    private Intent createStopIntent() {
        Intent intent = new Intent();
        intent.putExtra(InvalidationIntentProtocol.EXTRA_STOP, true);
        return intent;
    }

    /** Creates an intent to register some types with the InvalidationClientService. */
    private Intent createRegisterIntent(Account account, Set<Integer> types) {
        Intent intent = InvalidationIntentProtocol.createRegisterIntent(account, types);
        return intent;
    }

    /** Creates an intent to register some types with the InvalidationClientService. */
    private Intent createRegisterIntent(
            Account account, int[] objectSources, String[] objectNames) {
        Intent intent = InvalidationIntentProtocol.createRegisterIntent(
                account, objectSources, objectNames);
        return intent;
    }

    /** Returns whether {@code intent} is an {@link AndroidListener} start intent. */
    private boolean isAndroidListenerStartIntent(Intent intent) {
        Intent startIntent = AndroidListener.createStartIntent(getContext(),
                InvalidationClientService.CLIENT_TYPE,
                ApiCompatibilityUtils.getBytesUtf8("unused"));
        return intent.getExtras().keySet().equals(startIntent.getExtras().keySet());
    }

    /** Returns whether {@code intent} is an {@link AndroidListener} stop intent. */
    private boolean isAndroidListenerStopIntent(Intent intent) {
        Intent stopIntent = AndroidListener.createStopIntent(getContext());
        return intent.getExtras().keySet().equals(stopIntent.getExtras().keySet());
    }
}
