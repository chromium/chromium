// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.List;

/** Unit tests for {@link AdaptiveUiControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AdaptiveUiControllerImplTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final UnownedUserDataHost mHost = new UnownedUserDataHost();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AdaptiveUiObserver mObserver;

    private Activity mActivity;
    private AdaptiveUiControllerImpl mController;
    private BottomSheetContent mCurrentSheetContent;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mHost);
        when(mBottomSheetController.getCurrentSheetContent())
                .thenAnswer(invocation -> mCurrentSheetContent);
        // Behave like ModalDialogManager: dismissing a dialog reports it to its controller.
        doAnswer(
                        invocation -> {
                            PropertyModel model = invocation.getArgument(0);
                            model.get(ModalDialogProperties.CONTROLLER)
                                    .onDismiss(model, invocation.getArgument(1));
                            return null;
                        })
                .when(mModalDialogManager)
                .dismissDialog(any(), anyInt());
        mController =
                new AdaptiveUiControllerImpl(
                        mWindowAndroid, mModalDialogManager, mBottomSheetController);
    }

    @After
    public void tearDown() {
        mController.destroy();
        DeviceInfo.resetIsDesktopForTesting();
    }

    private AdaptiveUiEntry newEntry(@AdaptiveUiScope int scope, int... containers) {
        AdaptiveUiEntry.Builder builder =
                new AdaptiveUiEntry.Builder(
                                new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS).build())
                        .setScope(scope)
                        .setObserver(mObserver);
        for (int container : containers) builder.addAllowedContainer(container);
        return builder.build();
    }

    private AdaptiveUiEntry newEntry() {
        return newEntry(
                AdaptiveUiScope.TRANSIENT,
                AdaptiveUiContainer.BOTTOM_SHEET,
                AdaptiveUiContainer.MODAL_DIALOG);
    }

    private PropertyModel captureDialogModel(@ModalDialogType int type) {
        ArgumentCaptor<PropertyModel> captor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(captor.capture(), eq(type));
        return captor.getValue();
    }

    private ModalDialogManagerObserver captureDialogObserver() {
        ArgumentCaptor<ModalDialogManagerObserver> captor =
                ArgumentCaptor.forClass(ModalDialogManagerObserver.class);
        verify(mModalDialogManager).addObserver(captor.capture());
        return captor.getValue();
    }

    private BottomSheetContent captureSheetContent() {
        ArgumentCaptor<BottomSheetContent> captor =
                ArgumentCaptor.forClass(BottomSheetContent.class);
        verify(mBottomSheetController)
                .requestShowContent(captor.capture(), eq(/* animate= */ true));
        return captor.getValue();
    }

    private BottomSheetObserver captureSheetObserver() {
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController).addObserver(captor.capture());
        return captor.getValue();
    }

    // ---------------------------------------------------------------------------------------
    // Attaching.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testAttachesToWindowUntilDestroyed() {
        assertSame(mController, AdaptiveUiController.from(mWindowAndroid));

        mController.destroy();

        assertNull(AdaptiveUiController.from(mWindowAndroid));
    }

    @Test
    public void testDoesNotObserveContainersWhileIdle() {
        verify(mModalDialogManager, never()).addObserver(any());
        verify(mBottomSheetController, never()).addObserver(any());
    }

    // ---------------------------------------------------------------------------------------
    // Container selection.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testSelectContainer_DesktopPrefersDialog() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        assertEquals(
                AdaptiveUiContainer.MODAL_DIALOG,
                AdaptiveUiControllerImpl.selectContainer(
                        AdaptiveUiContainer.BOTTOM_SHEET | AdaptiveUiContainer.MODAL_DIALOG));
        assertEquals(
                AdaptiveUiContainer.BOTTOM_SHEET,
                AdaptiveUiControllerImpl.selectContainer(AdaptiveUiContainer.BOTTOM_SHEET));
    }

    @Test
    public void testSelectContainer_NonDesktopPrefersSheet() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        assertEquals(
                AdaptiveUiContainer.BOTTOM_SHEET,
                AdaptiveUiControllerImpl.selectContainer(
                        AdaptiveUiContainer.BOTTOM_SHEET | AdaptiveUiContainer.MODAL_DIALOG));
        assertEquals(
                AdaptiveUiContainer.MODAL_DIALOG,
                AdaptiveUiControllerImpl.selectContainer(AdaptiveUiContainer.MODAL_DIALOG));
    }

    @Test
    public void testSelectContainer_SidePanelOnlyThrows() {
        assertThrows(
                IllegalArgumentException.class,
                () -> AdaptiveUiControllerImpl.selectContainer(AdaptiveUiContainer.SIDE_PANEL));
    }

    @Test
    public void testShow_ContainerChosenPerShow() {
        // The form factor is read on each show(); an entry never moves once it is showing.
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        AdaptiveUiEntry first = newEntry();
        mController.show(first);
        verify(mObserver).onWillShow(AdaptiveUiContainer.BOTTOM_SHEET);

        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        mController.show(first);
        verify(mObserver, never()).onWillShow(AdaptiveUiContainer.MODAL_DIALOG);
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());

        mController.show(newEntry());
        verify(mObserver).onWillShow(AdaptiveUiContainer.MODAL_DIALOG);
    }

    // ---------------------------------------------------------------------------------------
    // Dialog path.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testDialog_WindowScopeIsAppModal() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        mController.show(newEntry(AdaptiveUiScope.WINDOW, AdaptiveUiContainer.MODAL_DIALOG));
        captureDialogModel(ModalDialogType.APP);
    }

    @Test
    public void testDialog_OnShownRelayedWhenContainerShowsIt() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        mController.show(newEntry());
        PropertyModel model = captureDialogModel(ModalDialogType.TAB);
        verify(mObserver).onWillShow(AdaptiveUiContainer.MODAL_DIALOG);
        verify(mObserver, never()).onShown();

        ModalDialogManagerObserver dialogObserver = captureDialogObserver();
        dialogObserver.onDialogCreated(model, /* dialog= */ null);
        dialogObserver.onDialogShown(new View(mActivity));

        verify(mObserver).onShown();
    }

    @Test
    public void testDialog_HideRelaysFeatureReason() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        AdaptiveUiEntry entry = newEntry();
        mController.show(entry);
        PropertyModel model = captureDialogModel(ModalDialogType.TAB);

        mController.hide(entry, DismissReason.PRIMARY_ACTION);

        verify(mModalDialogManager).dismissDialog(eq(model), anyInt());
        verify(mObserver).onDismissed(DismissReason.PRIMARY_ACTION);
    }

    @Test
    public void testDialog_ContainerDismissalRelayed() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        mController.show(newEntry());
        PropertyModel model = captureDialogModel(ModalDialogType.TAB);

        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.TAB_SWITCHED);

        verify(mObserver).onDismissed(DismissReason.TAB_SWITCHED);
    }

    // ---------------------------------------------------------------------------------------
    // Sheet path.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testSheet_OnWillShowBeforeRequestAndOnShownWhenVisible() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        mController.show(newEntry());

        InOrder inOrder = inOrder(mObserver, mBottomSheetController);
        inOrder.verify(mObserver).onWillShow(AdaptiveUiContainer.BOTTOM_SHEET);
        inOrder.verify(mBottomSheetController).requestShowContent(any(), eq(/* animate= */ true));
        verify(mObserver, never()).onShown();

        captureSheetObserver().onSheetContentChanged(captureSheetContent());
        verify(mObserver).onShown();
    }

    @Test
    public void testSheet_SuppressedSheetIsNotDismissed() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        mController.show(newEntry());
        BottomSheetContent content = captureSheetContent();
        BottomSheetObserver sheetObserver = captureSheetObserver();
        mCurrentSheetContent = content;

        sheetObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.OMNIBOX_FOCUS);

        verify(mObserver, never()).onDismissed(anyInt());
    }

    @Test
    public void testSheet_DestroyedByContainerRelaysLastHiddenReason() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        mController.show(newEntry());
        BottomSheetContent content = captureSheetContent();
        mCurrentSheetContent = content;

        captureSheetObserver().onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);
        content.destroy();

        verify(mObserver).onDismissed(DismissReason.SWIPE_DOWN);
    }

    @Test
    public void testSheet_HideShowingSheetRelaysFeatureReason() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        AdaptiveUiEntry entry = newEntry();
        mController.show(entry);
        BottomSheetContent content = captureSheetContent();
        mCurrentSheetContent = content;

        mController.hide(entry, DismissReason.SECONDARY_ACTION);
        verify(mBottomSheetController)
                .hideContent(
                        eq(content),
                        eq(/* animate= */ true),
                        eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mObserver, never()).onDismissed(anyInt());

        content.destroy();
        verify(mObserver).onDismissed(DismissReason.SECONDARY_ACTION);
    }

    @Test
    public void testSheet_InSheetCloseButtonClosesSheet() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        mController.show(newEntry());
        BottomSheetContent content = captureSheetContent();
        mCurrentSheetContent = content;

        content.getContentView().findViewById(R.id.adaptive_ui_sheet_close_button).performClick();

        verify(mBottomSheetController)
                .hideContent(
                        eq(content), eq(/* animate= */ true), eq(StateChangeReason.CLOSE_BUTTON));
        content.destroy();
        verify(mObserver).onDismissed(DismissReason.CLOSE_BUTTON);
    }

    // ---------------------------------------------------------------------------------------
    // Lifecycle.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testHideAfterDismissalIsIgnored() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        AdaptiveUiEntry entry = newEntry();
        mController.show(entry);
        mController.hide(entry, DismissReason.CLOSE_BUTTON);
        mController.hide(entry, DismissReason.CLOSE_BUTTON);

        verify(mObserver).onDismissed(anyInt());
    }

    @Test
    public void testDestroyClosesShowingAndQueuedEntries() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        mController.show(newEntry());
        mCurrentSheetContent = captureSheetContent();
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        mController.show(newEntry());

        mController.destroy();

        verify(mBottomSheetController).hideContent(any(), eq(/* animate= */ false), anyInt());
        verify(mModalDialogManager).dismissDialog(any(), anyInt());
        verify(mObserver, times(2)).onDismissed(DismissReason.TAB_CLOSED);
    }

    @Test
    public void testDestroyClosesQueuedEntriesBeforeShowingOnes() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        AdaptiveUiEntry showing = newEntry();
        mController.show(showing);
        BottomSheetContent content = captureSheetContent();
        mCurrentSheetContent = content;
        captureSheetObserver().onSheetContentChanged(content);
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        mController.show(newEntry());

        mController.destroy();

        // The queued dialog is closed first, so closing the sheet can't bring it on screen.
        InOrder inOrder = inOrder(mModalDialogManager, mBottomSheetController);
        inOrder.verify(mModalDialogManager).dismissDialog(any(), anyInt());
        inOrder.verify(mBottomSheetController)
                .hideContent(eq(content), eq(/* animate= */ false), anyInt());
        verify(mObserver).onShown();
        verify(mObserver, times(2)).onDismissed(DismissReason.TAB_CLOSED);
    }

    @Test
    public void testDestroyClosesSuspendedDialogBeforeCurrentOne() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        mController.show(newEntry(AdaptiveUiScope.TRANSIENT, AdaptiveUiContainer.MODAL_DIALOG));
        mController.show(newEntry(AdaptiveUiScope.WINDOW, AdaptiveUiContainer.MODAL_DIALOG));
        PropertyModel tabModel = captureDialogModel(ModalDialogType.TAB);
        PropertyModel appModel = captureDialogModel(ModalDialogType.APP);
        ArgumentCaptor<ModalDialogManagerObserver> captor =
                ArgumentCaptor.forClass(ModalDialogManagerObserver.class);
        verify(mModalDialogManager, times(2)).addObserver(captor.capture());

        // The tab-modal dialog is shown, then the app-modal one takes its place, and
        // ModalDialogManager puts the tab-modal one back in its queue without telling observers.
        for (PropertyModel model : List.of(tabModel, appModel)) {
            for (ModalDialogManagerObserver observer : captor.getAllValues()) {
                observer.onDialogCreated(model, /* dialog= */ null);
                observer.onDialogShown(new View(mActivity));
            }
        }

        mController.destroy();

        // The suspended dialog is closed first, so closing the current one can't bring it back.
        InOrder inOrder = inOrder(mModalDialogManager);
        inOrder.verify(mModalDialogManager).dismissDialog(eq(tabModel), anyInt());
        inOrder.verify(mModalDialogManager).dismissDialog(eq(appModel), anyInt());
        verify(mObserver, times(2)).onShown();
        verify(mObserver, times(2)).onDismissed(DismissReason.TAB_CLOSED);
    }

    @Test
    public void testDestroyClosesRequeuedSheetBeforeCurrentOne() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        mController.show(newEntry(AdaptiveUiScope.TRANSIENT, AdaptiveUiContainer.BOTTOM_SHEET));
        mController.show(newEntry(AdaptiveUiScope.WINDOW, AdaptiveUiContainer.BOTTOM_SHEET));
        ArgumentCaptor<BottomSheetContent> contentCaptor =
                ArgumentCaptor.forClass(BottomSheetContent.class);
        verify(mBottomSheetController, times(2))
                .requestShowContent(contentCaptor.capture(), eq(/* animate= */ true));
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController, times(2)).addObserver(observerCaptor.capture());
        BottomSheetContent first = contentCaptor.getAllValues().get(0);
        BottomSheetContent second = contentCaptor.getAllValues().get(1);

        // The first sheet is shown, then another sheet takes its place and the first one goes back
        // in the queue. Once that sheet closes, the second one is shown and the first one waits.
        for (BottomSheetContent content : List.of(first, second)) {
            mCurrentSheetContent = content;
            for (BottomSheetObserver observer : observerCaptor.getAllValues()) {
                observer.onSheetContentChanged(content);
            }
        }

        mController.destroy();

        // The requeued sheet is closed first, so closing the current one can't bring it back.
        InOrder inOrder = inOrder(mBottomSheetController);
        inOrder.verify(mBottomSheetController)
                .hideContent(eq(first), eq(/* animate= */ false), anyInt());
        inOrder.verify(mBottomSheetController)
                .hideContent(eq(second), eq(/* animate= */ false), anyInt());
        verify(mObserver, times(2)).onShown();
        verify(mObserver, times(2)).onDismissed(DismissReason.TAB_CLOSED);
    }

    @Test
    public void testHideFromOnWillShowSkipsTheContainer() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        AdaptiveUiEntry entry = newEntry();
        doAnswer(
                        invocation -> {
                            mController.hide(entry, DismissReason.CLOSE_BUTTON);
                            return null;
                        })
                .when(mObserver)
                .onWillShow(anyInt());

        mController.show(entry);

        verify(mObserver).onDismissed(DismissReason.CLOSE_BUTTON);
        verify(mObserver, never()).onShown();
        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
    public void testDestroyFromOnWillShowSkipsTheContainer() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        AdaptiveUiEntry entry = newEntry();
        doAnswer(
                        invocation -> {
                            mController.destroy();
                            return null;
                        })
                .when(mObserver)
                .onWillShow(anyInt());

        mController.show(entry);

        verify(mObserver).onDismissed(DismissReason.TAB_CLOSED);
        verify(mObserver, never()).onShown();
        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
    public void testHideAndShowAgainFromOnWillShowShowsOnce() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        AdaptiveUiEntry entry = newEntry();
        boolean[] reshown = new boolean[1];
        doAnswer(
                        invocation -> {
                            if (reshown[0]) return null;
                            reshown[0] = true;
                            mController.hide(entry, DismissReason.CLOSE_BUTTON);
                            mController.show(entry);
                            return null;
                        })
                .when(mObserver)
                .onWillShow(anyInt());

        mController.show(entry);

        // The nested show() created the presenter, so the outer one doesn't create another.
        verify(mObserver, times(2)).onWillShow(AdaptiveUiContainer.BOTTOM_SHEET);
        verify(mObserver).onDismissed(DismissReason.CLOSE_BUTTON);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testShowAgainAfterDismissalCreatesNewPresenter() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        AdaptiveUiEntry entry = newEntry();
        mController.show(entry);
        mController.hide(entry, DismissReason.CLOSE_BUTTON);

        mController.show(entry);

        verify(mBottomSheetController, times(2)).requestShowContent(any(), anyBoolean());
    }
}
