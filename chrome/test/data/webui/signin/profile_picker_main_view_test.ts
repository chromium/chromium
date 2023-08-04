// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/profile_picker.js';

import {loadTimeData, ManageProfilesBrowserProxyImpl, NavigationMixin, ProfileCardElement, ProfilePickerMainViewElement, ProfileState, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

class NavigationElement extends NavigationMixin
(PolymerElement) {
  static get is() {
    return 'navigation-element';
  }

  changeCalled: boolean = false;
  route: string = '';

  override ready() {
    super.ready();
    this.reset();
  }

  override onRouteChange(route: Routes, _step: string) {
    this.changeCalled = true;
    this.route = route;
  }

  reset() {
    this.changeCalled = false;
    this.route = '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'navigation-element': NavigationElement;
  }
}

customElements.define(NavigationElement.is, NavigationElement);

suite('ProfilePickerMainViewTest', function() {
  let mainViewElement: ProfilePickerMainViewElement;
  let browserProxy: TestManageProfilesBrowserProxy;
  let navigationElement: NavigationElement;

  function resetTest() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    navigationElement = document.createElement('navigation-element');
    document.body.appendChild(navigationElement);
    mainViewElement = document.createElement('profile-picker-main-view');
    document.body.appendChild(mainViewElement);
    return waitBeforeNextRender(mainViewElement);
  }

  function resetPolicies() {
    // This is necessary as |loadTimeData| state leaks between tests.
    // Any load time data manipulated by the tests needs to be reset here.
    loadTimeData.overrideValues({
      isGuestModeEnabled: true,
      isProfileCreationAllowed: true,
      isAskOnStartupAllowed: true,
    });
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    resetPolicies();
    return resetTest();
  });

  /**
   * @param n Indicates the desired number of profiles.
   */
  function generateProfilesList(n: number): ProfileState[] {
    return Array(n)
        .fill(0)
        .map((_x, i) => i % 2 === 0)
        .map((sync, i) => ({
               profilePath: `profilePath${i}`,
               localProfileName: `profile${i}`,
               isSyncing: sync,
               needsSignin: false,
               gaiaName: sync ? `User${i}` : '',
               userName: sync ? `User${i}@gmail.com` : '',
               isManaged: i % 4 === 0,
               avatarIcon: `AvatarUrl-${i}`,
               // <if expr="chromeos_lacros">
               isPrimaryLacrosProfile: false,
               // </if>
             }));
  }

  async function verifyProfileCard(
      expectedProfiles: ProfileState[],
      profiles: NodeListOf<ProfileCardElement>) {
    assertEquals(expectedProfiles.length, profiles.length);
    for (let i = 0; i < expectedProfiles.length; i++) {
      const profile = profiles[i]!;
      const expectedProfile = expectedProfiles[i]!;
      assertTrue(!!profile.shadowRoot!.querySelector('profile-card-menu'));
      profile.shadowRoot!.querySelector('cr-button')!.click();
      await browserProxy.whenCalled('launchSelectedProfile');
      assertEquals(
          profile.shadowRoot!
              .querySelector<HTMLElement>('#forceSigninContainer')!.hidden,
          !expectedProfile.needsSignin);

      const gaiaName = profile.$.gaiaName;
      assertEquals(gaiaName.hidden, expectedProfile.needsSignin);
      assertEquals(gaiaName.innerText.trim(), expectedProfile.gaiaName);

      assertEquals(profile.$.nameInput.value, expectedProfile.localProfileName);
      assertEquals(
          profile.shadowRoot!.querySelector<HTMLElement>(
                                 '#iconContainer')!.hidden,
          !expectedProfile.isManaged);
      assertEquals(
          (profile.shadowRoot!
               .querySelector<HTMLImageElement>('.profile-avatar')!.src)
              .split('/')
              .pop(),
          expectedProfile.avatarIcon);
    }
  }

  test('MainViewWithDefaultPolicies', async function() {
    assertTrue(navigationElement.changeCalled);
    assertEquals(navigationElement.route, Routes.MAIN);
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    const profiles = generateProfilesList(6);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    // Profiles list defined.
    assertTrue(!mainViewElement.$.profilesContainer.hidden);
    assertTrue(!mainViewElement.$.askOnStartup.hidden);
    assertTrue(mainViewElement.$.askOnStartup.checked);
    // Verify profile card.
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    // Browse as guest.
    assertTrue(!!mainViewElement.$.browseAsGuestButton);
    mainViewElement.$.browseAsGuestButton.click();
    await browserProxy.whenCalled('launchGuestProfile');
    // Ask when chrome opens.
    mainViewElement.$.askOnStartup.click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(!mainViewElement.$.askOnStartup.checked);
    // Update profile data.
    profiles[1] = profiles[4]!;
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    // Profiles update on remove.
    webUIListenerCallback('profile-removed', profiles[3]!.profilePath);
    profiles.splice(3, 1);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
  });

  test('EditLocalProfileName', async function() {
    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(1);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    const localProfileName =
        mainViewElement.shadowRoot!.querySelector('profile-card')!.$.nameInput;
    assertEquals(localProfileName.value, profiles[0]!.localProfileName);

    // Set to valid profile name.
    localProfileName.value = 'Alice';
    localProfileName.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    const args = await browserProxy.whenCalled('setProfileName');
    assertEquals(args[0], profiles[0]!.profilePath);
    assertEquals(args[1], 'Alice');
    assertEquals(localProfileName.value, 'Alice');

    // Set to invalid profile name
    localProfileName.value = '';
    assertTrue(localProfileName.invalid);
  });

  test('GuestModeDisabled', async function() {
    loadTimeData.overrideValues({
      isGuestModeEnabled: false,
    });
    resetTest();
    assertEquals(mainViewElement.$.browseAsGuestButton.style.display, 'none');
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback('profiles-list-changed', generateProfilesList(2));
    flushTasks();
    assertEquals(mainViewElement.$.browseAsGuestButton.style.display, 'none');
  });

  test('ProfileCreationNotAllowed', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: false,
    });
    resetTest();
    const addProfile =
        mainViewElement.shadowRoot!.querySelector<HTMLElement>('#addProfile')!;
    assertEquals(addProfile.style.display, 'none');
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback('profiles-list-changed', generateProfilesList(2));
    flushTasks();
    navigationElement.reset();
    assertEquals(addProfile.style.display, 'none');
    addProfile.click();
    flushTasks();
    assertTrue(!navigationElement.changeCalled);
  });

  test('AskOnStartupSingleToMultipleProfiles', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    let profiles = generateProfilesList(1);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    // The checkbox 'Ask when chrome opens' should only be visible to
    // multi-profile users.
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    // Add a second profile.
    profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.$.askOnStartup.hidden);
    assertTrue(mainViewElement.$.askOnStartup.checked);
    mainViewElement.$.askOnStartup.click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(!mainViewElement.$.askOnStartup.checked);
  });

  test('AskOnStartupMultipleToSingleProfile', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    const profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.$.askOnStartup.hidden);
    // Remove profile.
    webUIListenerCallback('profile-removed', profiles[0]!.profilePath);
    flushTasks();
    await verifyProfileCard(
        [profiles[1]!],
        mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
    assertTrue(mainViewElement.$.askOnStartup.hidden);
  });

  test('AskOnStartupMulipleProfiles', async function() {
    // Disable AskOnStartup
    loadTimeData.overrideValues({isAskOnStartupAllowed: false});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.$.profilesContainer.hidden);
    assertTrue(mainViewElement.$.askOnStartup.hidden);
    const profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));

    // Checkbox hidden even if there are multiple profiles.
    assertTrue(mainViewElement.$.askOnStartup.hidden);
  });

  test('ForceSigninIsEnabled', async function() {
    loadTimeData.overrideValues({isForceSigninEnabled: true});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(2);
    profiles[0]!.needsSignin = true;
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot!.querySelectorAll('profile-card'));
  });

  // This function makes sure that the test data is valid and consistent.
  function checkTestData(
      numberOfProfiles: number, dragIndex: number,
      dragEnterEvents:
          Array<{index: number, expectedResultIndices: number[]}>) {
    assertTrue(
        0 <= dragIndex && dragIndex < numberOfProfiles,
        'Test setup error: Drag index is out of bounds.');

    dragEnterEvents.forEach(event => {
      assertTrue(
          0 <= event.index && event.index < numberOfProfiles,
          'Test setup error:  Event index is out of bounds');
      const indicesSet = new Set();
      event.expectedResultIndices.forEach(resultIndex => {
        assertTrue(
            0 <= resultIndex && resultIndex < numberOfProfiles,
            'Test setup error:  Expected index result is out of bounds');
        assertFalse(
            indicesSet.has(resultIndex),
            'Test setup error: Expected indices should\'nt have a repeated index');
        indicesSet.add(resultIndex);
      });
    });
  }

  // This function tests the current DOMCRect of the elements in `cards` with
  // their expected positions in `expectedIndices` vs their initial positions
  // stored in `initialRects` with their initial position indices (that are
  // simply ordered).
  // This check works because we expect profile cards to take the spot of other
  // cards when they move due to drag events.
  function assertProfilesPositions(
      cards: ProfileCardElement[], draggingIndex: number|null,
      initialRects: DOMRect[], expectedIndices: number[]) {
    for (let i = 0; i < expectedIndices.length; ++i) {
      const expectedIndex = expectedIndices[i]!;
      // Do not compare the dragging profile element position, as it does not
      // move, it is only hidden.
      if (draggingIndex != null && draggingIndex === expectedIndex) {
        assertTrue(cards[draggingIndex]!.classList.contains('dragging'));
        continue;
      }

      // Initial indices are ordered, so we can use the loop index as the
      // basis.
      assertDeepEquals(
          cards[expectedIndex]!.getBoundingClientRect(), initialRects[i]);
    }
  }

  // Sets up the profile picker with the reorder functionality and creates
  // profiles.
  async function setupProfileReorderingTest(numberOfProfiles: number) {
    // Activates the profile reordering feature.
    loadTimeData.overrideValues({profilesReorderingEnabled: true});

    // Remove transition duration to avoid waiting during tests.
    mainViewElement.setDraggingTransitionDurationForTesting(0);

    // Create the profiles and push them to the main profile picker view.
    const profiles = generateProfilesList(numberOfProfiles);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();

    // Make sure to wait for the initialization of the DragDelegate which
    // happens once the profile cards are rendered.
    await waitAfterNextRender(mainViewElement.$.profiles);
  }

  // This test function simulates a full drag event cycle.
  // It first creates multiple profiles based on `numberOfProfiles` and
  // initialize the profile picker main view then perform some drag events:
  // - dragstart: with the given `dragIndex`.
  // - dragenter: multiple events through `dragEnterEvents`.
  // - dragend: with the given `dragIndex`.
  //
  // Multiple checks are done through the test to guarantee the state after each
  // drag event.
  //
  // Variables:
  // - `numberOfProfiles`: number of profiles created for the drag cycle.
  // - `dragIndex`: the index of the card being dragged in this cycle.
  // - `dragEnterEvents`: the list of drag enters that will happen in the cycle.
  //    - `index`: the index of the card that will be entered.
  //    - `expectedResultIndices`: the expected indices of profiles after the
  //    drag enter event.
  async function testProfileReorderingDragCycle(dragData: {
    numberOfProfiles: number,
    dragIndex: number,
    dragEnterEvents: Array<{index: number, expectedResultIndices: number[]}>,
  }) {
    // Preliminary `dragData` checks before proceeding with the actual test.
    checkTestData(
        dragData.numberOfProfiles, dragData.dragIndex,
        dragData.dragEnterEvents);

    await setupProfileReorderingTest(dragData.numberOfProfiles);

    const cards = Array.from(
        mainViewElement.shadowRoot!.querySelectorAll<ProfileCardElement>(
            'profile-card'));

    // Store the initial profile cards rects for later comparison.
    const initialRects =
        cards.map(card => card.getBoundingClientRect()) as DOMRect[];

    // Equivalent to an array {0, 1, 2. ... , numberOfProfiles - 1}.
    const initialIndices = Array.from(Array(dragData.numberOfProfiles).keys());
    // Check that the initial positions of the profiles are aligned.
    assertProfilesPositions(cards, null, initialRects, initialIndices);

    // ----------------- Start of the Drag Events -----------------

    // - Perform the Drag Start.
    cards[dragData.dragIndex]!.dispatchEvent(new DragEvent('dragstart'));
    assertTrue(cards[dragData.dragIndex]!.classList.contains('dragging'));

    // - Perform the list of Drag Enter events with position checks.
    dragData.dragEnterEvents.forEach(event => {
      // Perform the drag enter event.
      cards[event.index]!.dispatchEvent(new DragEvent('dragenter'));
      // Check that the positions of the cards are as expected after each enter
      // events.
      assertProfilesPositions(
          cards, dragData.dragIndex, initialRects, event.expectedResultIndices);
    });

    // - Perform the Drag End.
    cards[dragData.dragIndex]!.dispatchEvent(new DragEvent('dragend'));
    assertFalse(cards[dragData.dragIndex]!.classList.contains('dragging'));
  }

  test('ProfileReorder_DragStartEndNoEnter', async function() {
    await testProfileReorderingDragCycle(
        {numberOfProfiles: 3, dragIndex: 1, dragEnterEvents: []});
  });

  // This test simulates the dragged card to generate a 'dragenter' event on
  // itself (the hidden dragging card). This should have no effect on the order.
  test('ProfileReorder_DragEnterOnDraggedCardHasNoEffect', async function() {
    await testProfileReorderingDragCycle({
      numberOfProfiles: 3,
      dragIndex: 1,
      dragEnterEvents: [{index: 1, expectedResultIndices: [0, 1, 2]}],
    });
  });

  test('ProfileReorder_DragEnterOnNextCard', async function() {
    await testProfileReorderingDragCycle({
      numberOfProfiles: 3,
      dragIndex: 1,
      dragEnterEvents: [{index: 2, expectedResultIndices: [0, 2, 1]}],
    });
  });

  test('ProfileReorder_DragEnterOnFurtherCard', async function() {
    await testProfileReorderingDragCycle({
      numberOfProfiles: 3,
      dragIndex: 0,
      dragEnterEvents: [{index: 2, expectedResultIndices: [1, 2, 0]}],
    });
  });

  test('ProfileReorder_DragMultipleEnters', async function() {
    await testProfileReorderingDragCycle({
      numberOfProfiles: 4,
      dragIndex: 3,
      dragEnterEvents: [
        {index: 2, expectedResultIndices: [0, 1, 3, 2]},
        {index: 1, expectedResultIndices: [0, 3, 1, 2]},
        {index: 0, expectedResultIndices: [3, 0, 1, 2]},
      ],
    });
  });

  // This test makes sure that if any draggable object (not a profile) triggers
  // a `dragenter` event for a profile-card the reordering is not triggered and
  // the profile order is not affected.
  test('ProfileReorder_DragEnterOutOfDragCycleHasNoEffect', async function() {
    setupProfileReorderingTest(3);

    const cards = Array.from(
        mainViewElement.shadowRoot!.querySelectorAll<ProfileCardElement>(
            'profile-card'));

    // Store the initial profile cards rects for later comparison.
    const initialRects =
        cards.map(card => card.getBoundingClientRect()) as DOMRect[];
    const initiIndices = [0, 1, 2];
    assertProfilesPositions(cards, null, initialRects, initiIndices);

    // Simulate a dragenter event without having a prior dragstart that started
    // the drag cycle event.
    cards[0]!.dispatchEvent(new DragEvent('dragenter'));

    // Same assertion as the drag event should have no effect, or cause no
    // crash.
    assertProfilesPositions(cards, null, initialRects, initiIndices);
  });

  test('ProfilesDragSingleEnterWithReset', async function() {
    // Note that in this test, both "reset" enter events (the second event in
    // both cases) works whether it is the moved card or the dragging card. Both
    // result in the same behavior; having the initial state as if there was no
    // shift. This is expected, because right before the reset, the cards are on
    // top of each other, and we expect that if any of those are entered to have
    // the same behavior.

    // Last enter event enters the shifted card.
    await testProfileReorderingDragCycle({
      numberOfProfiles: 3,
      dragIndex: 1,
      dragEnterEvents: [
        {index: 2, expectedResultIndices: [0, 2, 1]},
        {index: 1, expectedResultIndices: [0, 1, 2]},
      ],
    });

    resetTest();

    // Last enter event enters the dragging card.
    await testProfileReorderingDragCycle({
      numberOfProfiles: 3,
      dragIndex: 1,
      dragEnterEvents: [
        {index: 2, expectedResultIndices: [0, 2, 1]},
        {index: 2, expectedResultIndices: [0, 1, 2]},
      ],
    });
  });

  test('ProfilesDragMultipleEntersWithResets', async function() {
    await testProfileReorderingDragCycle({
      numberOfProfiles: 4,
      dragIndex: 0,
      dragEnterEvents: [
        {index: 1, expectedResultIndices: [1, 0, 2, 3]},
        {index: 2, expectedResultIndices: [1, 2, 0, 3]},
        {index: 3, expectedResultIndices: [1, 2, 3, 0]},
        {index: 3, expectedResultIndices: [1, 2, 0, 3]},
        {index: 2, expectedResultIndices: [1, 0, 2, 3]},
        {index: 1, expectedResultIndices: [0, 1, 2, 3]},
      ],
    });
  });

  test(
      'ProfilesDragMultipleEntersOnDifferentSidesOfTheDraggingCard',
      async function() {
        await testProfileReorderingDragCycle({
          numberOfProfiles: 4,
          dragIndex: 2,
          dragEnterEvents: [
            {index: 3, expectedResultIndices: [0, 1, 3, 2]},
            {index: 0, expectedResultIndices: [2, 0, 1, 3]},
          ],
        });
      });
});
