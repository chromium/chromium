// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Coordinator for the enterprise signals disclaimer UI.
 *
 * <p>This coordinator is only responsible for rendering the disclaimer content and notifying the
 * embedder, via {@link Delegate}, about the disclaimer being shown and about the user interaction.
 * Hosting the view returned by {@link #getView()} (e.g. in a bottom sheet or a modal dialog) and
 * acting on the user's decision is the responsibility of the embedder.
 */
@NullMarked
public class EnterpriseSignalsDisclaimerCoordinator implements View.OnAttachStateChangeListener {
    /** How the embedder is going to present the disclaimer. Affects the rendered layout. */
    @IntDef({PresentationMode.MODAL_DIALOG, PresentationMode.BOTTOM_SHEET})
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    public @interface PresentationMode {
        int MODAL_DIALOG = 0;
        int BOTTOM_SHEET = 1;
    }

    /** Delegate for the enterprise signals disclaimer. */
    public interface Delegate extends EnterpriseSignalsDisclaimerMediator.Delegate {
        /** Called the first time the disclaimer view is attached to a window. */
        default void onShown() {}
    }

    private final EnterpriseSignalsDisclaimerMediator mMediator;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final Delegate mDelegate;
    private final EnterpriseSignalsDisclaimerView mView;
    private boolean mIsDestroyed;

    /**
     * Constructs an {@link EnterpriseSignalsDisclaimerCoordinator}.
     *
     * <p>This class should only be instantiated for a managed account.
     *
     * @param context The Android {@link Context}.
     * @param identityManager The {@link IdentityManager} used to fetch the account information.
     * @param account The account the disclaimer is shown for.
     * @param presentationMode How the embedder is going to present the disclaimer.
     * @param delegate The {@link Delegate} for embedder interactions.
     */
    public EnterpriseSignalsDisclaimerCoordinator(
            Context context,
            IdentityManager identityManager,
            CoreAccountInfo account,
            @PresentationMode int presentationMode,
            Delegate delegate) {
        mDelegate = delegate;
        mView =
                new EnterpriseSignalsDisclaimerView(
                        context, /* isDialog= */ presentationMode == PresentationMode.MODAL_DIALOG);

        mView.addOnAttachStateChangeListener(this);

        mMediator =
                new EnterpriseSignalsDisclaimerMediator(
                        context, identityManager, account, delegate);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), mView, EnterpriseSignalsDisclaimerViewBinder::bind);
    }

    /** Returns the root view of the disclaimer, to be hosted by the embedder. */
    public View getView() {
        return mView;
    }

    /** Returns the vertical scroll offset of the disclaimer content. */
    int getVerticalScrollOffset() {
        return mView.getScrollViewScrollY();
    }

    /** Destroys the coordinator, cleaning up resources. Does not affect the hosting UI. */
    public void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        mView.removeOnAttachStateChangeListener(this);
        mModelChangeProcessor.destroy();
        mMediator.destroy();
    }

    // View.OnAttachStateChangeListener implementation.
    @Override
    public void onViewAttachedToWindow(View view) {
        if (mIsDestroyed) {
            return;
        }

        mDelegate.onShown();
        view.removeOnAttachStateChangeListener(this);
    }

    @Override
    public void onViewDetachedFromWindow(View view) {}
}
