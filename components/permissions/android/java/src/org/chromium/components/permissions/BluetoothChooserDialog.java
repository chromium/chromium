// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.location.LocationManager;
import android.text.SpannableString;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.content_public.browser.bluetooth.BluetoothChooserEvent;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A dialog for picking available Bluetooth devices. This dialog is shown when a website requests to
 * pair with a certain class of Bluetooth devices (e.g. through a bluetooth.requestDevice Javascript
 * call).
 *
 * <p>The dialog is shown by create() or show(), and always runs finishDialog() as it's closing.
 */
@JNINamespace("permissions")
public class BluetoothChooserDialog
        implements ItemChooserDialog.ItemSelectedCallback, PermissionCallback {
    private static final String TAG = "Bluetooth";

    // These constants match BluetoothChooserAndroid::ShowDiscoveryState, and are used in
    // notifyDiscoveryState().
    @IntDef({
        DiscoveryMode.DISCOVERY_FAILED_TO_START,
        DiscoveryMode.DISCOVERING,
        DiscoveryMode.DISCOVERY_IDLE
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public @interface DiscoveryMode {
        int DISCOVERY_FAILED_TO_START = 0;
        int DISCOVERING = 1;
        int DISCOVERY_IDLE = 2;
    }

    // The window that owns this dialog.
    final WindowAndroid mWindowAndroid;

    // Always equal to mWindowAndroid.getActivity().get(), but stored separately to make sure it's
    // not GC'ed.
    final Activity mActivity;

    // Always equal to mWindowAndroid.getContext().get(), but stored separately to make sure it's
    // not GC'ed.
    final Context mContext;

    // The dialog to show to let the user pick a device.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public ItemChooserDialog mItemChooserDialog;

    // The origin for the site wanting to pair with the bluetooth devices.
    final String mOrigin;

    // The security level of the connection to the site wanting to pair with the
    // bluetooth devices. For valid values see SecurityStateModel::SecurityLevel.
    final int mSecurityLevel;

    // The embedder-provided delegate.
    final BluetoothChooserAndroidDelegate mDelegate;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public Drawable mConnectedIcon;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public String mConnectedIconDescription;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public Drawable[] mSignalStrengthLevelIcon;

    // A pointer back to the native part of the implementation for this dialog.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public long mNativeBluetoothChooserDialogPtr;

    // Used to keep track of when the Mode Changed Receiver is registered.
    boolean mIsLocationModeChangedReceiverRegistered;

    // The local device Bluetooth adapter.
    private final BluetoothAdapter mAdapter;

    // The status message to show when the bluetooth adapter is turned off.
    private final SpannableString mAdapterOffStatus;

    // Should the "adapter off" message be shown once Bluetooth permission is granted?
    private boolean mAdapterOff;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public final BroadcastReceiver mLocationModeBroadcastReceiver =
            new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (!LocationManager.MODE_CHANGED_ACTION.equals(intent.getAction())) return;

                    if (!checkLocationServicesAndPermission()) return;

                    mItemChooserDialog.clear();

                    if (mAdapterOff) {
                        notifyAdapterTurnedOff();
                        return;
                    }

                    Natives jni = BluetoothChooserDialogJni.get();
                    jni.restartSearch(mNativeBluetoothChooserDialogPtr);
                }
            };

    // The type of link that is shown within the dialog.
    @IntDef({
        LinkType.EXPLAIN_BLUETOOTH,
        LinkType.ADAPTER_OFF,
        LinkType.ADAPTER_OFF_HELP,
        LinkType.REQUEST_PERMISSIONS,
        LinkType.REQUEST_LOCATION_SERVICES,
        LinkType.NEED_LOCATION_PERMISSION_HELP,
        LinkType.RESTART_SEARCH
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LinkType {
        int EXPLAIN_BLUETOOTH = 0;
        int ADAPTER_OFF = 1;
        int ADAPTER_OFF_HELP = 2;
        int REQUEST_PERMISSIONS = 3;
        int REQUEST_LOCATION_SERVICES = 4;
        int NEED_LOCATION_PERMISSION_HELP = 5;
        int RESTART_SEARCH = 6;
    }

    /** Creates the BluetoothChooserDialog. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public BluetoothChooserDialog(
            WindowAndroid windowAndroid,
            String origin,
            int securityLevel,
            BluetoothChooserAndroidDelegate delegate,
            long nativeBluetoothChooserDialogPtr) {
        mWindowAndroid = windowAndroid;
        mActivity = windowAndroid.getActivity().get();
        assert mActivity != null;
        mContext = windowAndroid.getContext().get();
        assert mContext != null;
        mOrigin = origin;
        mSecurityLevel = securityLevel;
        mDelegate = delegate;
        mNativeBluetoothChooserDialogPtr = nativeBluetoothChooserDialogPtr;
        mAdapter = BluetoothAdapter.getDefaultAdapter();

        // Initialize icons.
        mConnectedIcon = getIconWithRowIconColorStateList(R.drawable.ic_bluetooth_connected);
        mConnectedIconDescription = mContext.getString(R.string.bluetooth_device_connected);

        mSignalStrengthLevelIcon =
                new Drawable[] {
                    getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_0_bar),
                    getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_1_bar),
                    getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_2_bar),
                    getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_3_bar),
                    getIconWithRowIconColorStateList(R.drawable.ic_signal_cellular_4_bar)
                };

        if (mAdapter == null) {
            Log.i(TAG, "BluetoothChooserDialog: Default Bluetooth adapter not found.");
        }
        mAdapterOffStatus =
                SpanApplier.applySpans(
                        mContext.getString(R.string.bluetooth_adapter_off_help),
                        new SpanInfo(
                                "<link>", "</link>", createLinkSpan(LinkType.ADAPTER_OFF_HELP)));
    }

    private Drawable getIconWithRowIconColorStateList(int icon) {
        Resources res = mContext.getResources();

        Drawable drawable = TraceEventVectorDrawableCompat.create(res, icon, mContext.getTheme());
        DrawableCompat.setTintList(
                drawable,
                AppCompatResources.getColorStateList(
                        mContext, R.color.item_chooser_row_icon_color));
        return drawable;
    }

    /** Show the BluetoothChooserDialog. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void show() {
        SpannableString origin = new SpannableString(mOrigin);

        final boolean useDarkColors = !ColorUtils.inNightMode(mContext);
        AutocompleteSchemeClassifier autocompleteSchemeClassifier =
                mDelegate.createAutocompleteSchemeClassifier();

        OmniboxUrlEmphasizer.emphasizeUrl(
                origin,
                mContext,
                autocompleteSchemeClassifier,
                mSecurityLevel,
                useDarkColors,
                true);
        autocompleteSchemeClassifier.destroy();
        // Construct a full string and replace the origin text with emphasized version.
        SpannableString title =
                new SpannableString(mContext.getString(R.string.bluetooth_dialog_title, mOrigin));
        int start = title.toString().indexOf(mOrigin);
        TextUtils.copySpansFrom(origin, 0, origin.length(), Object.class, title, start);

        String noneFound = mContext.getString(R.string.bluetooth_not_found);

        SpannableString searching =
                SpanApplier.applySpans(
                        mContext.getString(R.string.bluetooth_searching),
                        new SpanInfo(
                                "<link>", "</link>", createLinkSpan(LinkType.EXPLAIN_BLUETOOTH)));

        String positiveButton = mContext.getString(R.string.bluetooth_confirm_button);

        SpannableString statusIdleNoneFound =
                SpanApplier.applySpans(
                        mContext.getString(R.string.bluetooth_not_seeing_it_idle),
                        new SpanInfo(
                                "<link1>", "</link1>", createLinkSpan(LinkType.EXPLAIN_BLUETOOTH)),
                        new SpanInfo(
                                "<link2>", "</link2>", createLinkSpan(LinkType.RESTART_SEARCH)));

        SpannableString statusActive = searching;

        SpannableString statusIdleSomeFound = statusIdleNoneFound;

        ItemChooserDialog.ItemChooserLabels labels =
                new ItemChooserDialog.ItemChooserLabels(
                        title,
                        searching,
                        noneFound,
                        statusActive,
                        statusIdleNoneFound,
                        statusIdleSomeFound,
                        positiveButton);
        mItemChooserDialog = new ItemChooserDialog(mContext, mActivity.getWindow(), this, labels);

        ContextUtils.registerProtectedBroadcastReceiver(
                mActivity,
                mLocationModeBroadcastReceiver,
                new IntentFilter(LocationManager.MODE_CHANGED_ACTION));
        mIsLocationModeChangedReceiverRegistered = true;
    }

    // Called to report the dialog's results back to native code.
    private void finishDialog(int resultCode, String id) {
        if (mIsLocationModeChangedReceiverRegistered) {
            mActivity.unregisterReceiver(mLocationModeBroadcastReceiver);
            mIsLocationModeChangedReceiverRegistered = false;
        }

        if (mNativeBluetoothChooserDialogPtr != 0) {
            Natives jni = BluetoothChooserDialogJni.get();
            jni.onDialogFinished(mNativeBluetoothChooserDialogPtr, resultCode, id);
        }
    }

    @Override
    public void onItemSelected(String id) {
        if (id.isEmpty()) {
            finishDialog(BluetoothChooserEvent.CANCELLED, "");
        } else {
            finishDialog(BluetoothChooserEvent.SELECTED, id);
        }
    }

    @Override
    public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
        // The chooser might have been closed during the request.
        if (mNativeBluetoothChooserDialogPtr == 0) return;

        if (!checkLocationServicesAndPermission()) return;

        mItemChooserDialog.clear();

        if (mAdapterOff) {
            notifyAdapterTurnedOff();
            return;
        }

        Natives jni = BluetoothChooserDialogJni.get();
        jni.restartSearch(mNativeBluetoothChooserDialogPtr);
    }

    // Returns true if Location Services is on and Chrome has permission to see the user's location.
    private boolean checkLocationServicesAndPermission() {
        final boolean havePermission =
                PermissionUtil.hasSystemPermissionsForBluetooth(mWindowAndroid);
        boolean needsLocationServices = PermissionUtil.needsLocationServicesForBluetooth();

        if (!havePermission
                && !PermissionUtil.canRequestSystemPermissionsForBluetooth(mWindowAndroid)) {
            // Immediately close the dialog because the user has asked Chrome not to request the
            // necessary permissions.
            finishDialog(BluetoothChooserEvent.DENIED_PERMISSION, "");
            return false;
        }

        // Compute the message to show the user.
        final SpanInfo permissionSpan =
                new SpanInfo(
                        "<permission_link>",
                        "</permission_link>",
                        createLinkSpan(LinkType.REQUEST_PERMISSIONS));
        final SpanInfo servicesSpan =
                new SpanInfo(
                        "<services_link>",
                        "</services_link>",
                        createLinkSpan(LinkType.REQUEST_LOCATION_SERVICES));
        final SpannableString needPermissionMessage;
        if (havePermission) {
            if (needsLocationServices) {
                needPermissionMessage =
                        SpanApplier.applySpans(
                                mContext.getString(R.string.bluetooth_need_location_services_on),
                                servicesSpan);
            } else {
                // We don't need to request anything.
                return true;
            }
        } else {
            if (needsLocationServices) {
                // If it needs locations services, it implicitly means it is a version
                // lower than Android S, so we can assume the system permission
                // needed is location permission.
                int resourceId = R.string.bluetooth_need_location_permission_and_services_on;
                needPermissionMessage =
                        SpanApplier.applySpans(
                                mContext.getString(resourceId), permissionSpan, servicesSpan);
            } else {
                if (PermissionUtil.needsNearbyDevicesPermissionForBluetooth(mWindowAndroid)) {
                    int resourceId = R.string.bluetooth_need_nearby_devices_permission;
                    needPermissionMessage =
                            SpanApplier.applySpans(mContext.getString(resourceId), permissionSpan);
                } else {
                    int resourceId = R.string.bluetooth_need_location_permission;
                    needPermissionMessage =
                            SpanApplier.applySpans(mContext.getString(resourceId), permissionSpan);
                }
            }
        }

        SpannableString needPermissionStatus =
                SpanApplier.applySpans(
                        mContext.getString(R.string.bluetooth_need_location_permission_help),
                        new SpanInfo(
                                "<link>",
                                "</link>",
                                createLinkSpan(LinkType.NEED_LOCATION_PERMISSION_HELP)));

        mItemChooserDialog.setErrorState(needPermissionMessage, needPermissionStatus);
        return false;
    }

    private NoUnderlineClickableSpan createLinkSpan(@LinkType int linkType) {
        return new NoUnderlineClickableSpan(
                mContext, (view) -> onBluetoothLinkClick(view, linkType));
    }

    private void onBluetoothLinkClick(View view, @LinkType int linkType) {
        if (mNativeBluetoothChooserDialogPtr == 0) return;

        Natives jni = BluetoothChooserDialogJni.get();

        switch (linkType) {
            case LinkType.EXPLAIN_BLUETOOTH:
                // No need to close the dialog here because ShowBluetoothOverviewLink will close
                // it.
                jni.showBluetoothOverviewLink(mNativeBluetoothChooserDialogPtr);
                break;
            case LinkType.ADAPTER_OFF:
                if (mAdapter != null && mAdapter.enable()) {
                    mItemChooserDialog.signalInitializingAdapter();
                } else {
                    String unableToTurnOnAdapter =
                            mContext.getString(R.string.bluetooth_unable_to_turn_on_adapter);
                    mItemChooserDialog.setErrorState(unableToTurnOnAdapter, mAdapterOffStatus);
                }
                break;
            case LinkType.ADAPTER_OFF_HELP:
                jni.showBluetoothAdapterOffLink(mNativeBluetoothChooserDialogPtr);
                break;
            case LinkType.REQUEST_PERMISSIONS:
                mItemChooserDialog.setIgnorePendingWindowFocusChangeForClose(true);
                PermissionUtil.requestSystemPermissionsForBluetooth(
                        mWindowAndroid, BluetoothChooserDialog.this);
                break;
            case LinkType.REQUEST_LOCATION_SERVICES:
                mItemChooserDialog.setIgnorePendingWindowFocusChangeForClose(true);
                PermissionUtil.requestLocationServices(mWindowAndroid);
                break;
            case LinkType.NEED_LOCATION_PERMISSION_HELP:
                jni.showNeedLocationPermissionLink(mNativeBluetoothChooserDialogPtr);
                break;
            case LinkType.RESTART_SEARCH:
                mItemChooserDialog.clear();
                jni.restartSearch(mNativeBluetoothChooserDialogPtr);
                break;
            default:
                assert false;
        }

        // Get rid of the highlight background on selection.
        view.invalidate();
    }

    @CalledByNative
    @VisibleForTesting
    public static BluetoothChooserDialog create(
            WindowAndroid windowAndroid,
            String origin,
            int securityLevel,
            BluetoothChooserAndroidDelegate delegate,
            long nativeBluetoothChooserDialogPtr) {
        if (!PermissionUtil.hasSystemPermissionsForBluetooth(windowAndroid)
                && !PermissionUtil.canRequestSystemPermissionsForBluetooth(windowAndroid)) {
            // If we can't even ask for enough permission to scan for Bluetooth devices, don't open
            // the dialog.
            return null;
        }

        // Avoid showing the chooser when ModalDialogManager indicates that
        // tab-modal or app-modal dialogs are suspended.
        // TODO(crbug.com/41483591): Integrate BluetoothChooserDialog with
        // ModalDialogManager.
        ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        if (modalDialogManager != null
                && (modalDialogManager.isSuspended(ModalDialogManager.ModalDialogType.TAB)
                        || modalDialogManager.isSuspended(
                                ModalDialogManager.ModalDialogType.APP))) {
            return null;
        }

        BluetoothChooserDialog dialog =
                new BluetoothChooserDialog(
                        windowAndroid,
                        origin,
                        securityLevel,
                        delegate,
                        nativeBluetoothChooserDialogPtr);
        dialog.show();
        return dialog;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    public void addOrUpdateDevice(
            String deviceId, String deviceName, boolean isGATTConnected, int signalStrengthLevel) {
        Drawable icon = null;
        String iconDescription = null;
        if (isGATTConnected) {
            icon = mConnectedIcon.getConstantState().newDrawable();
            iconDescription = mConnectedIconDescription;
        } else if (signalStrengthLevel != -1) {
            icon = mSignalStrengthLevelIcon[signalStrengthLevel].getConstantState().newDrawable();
            iconDescription =
                    mContext.getResources()
                            .getQuantityString(
                                    R.plurals.signal_strength_level_n_bars,
                                    signalStrengthLevel,
                                    signalStrengthLevel);
        }

        mItemChooserDialog.addOrUpdateItem(deviceId, deviceName, icon, iconDescription);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    public void closeDialog() {
        mNativeBluetoothChooserDialogPtr = 0;
        mItemChooserDialog.dismiss();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    public void notifyAdapterTurnedOff() {
        mAdapterOff = true;

        // Permission is required to turn the adapter on so make sure to ask for that first.
        if (checkLocationServicesAndPermission()) {
            SpannableString adapterOffMessage =
                    SpanApplier.applySpans(
                            mContext.getString(R.string.bluetooth_adapter_off),
                            new SpanInfo(
                                    "<link>", "</link>", createLinkSpan(LinkType.ADAPTER_OFF)));

            mItemChooserDialog.setErrorState(adapterOffMessage, mAdapterOffStatus);
        }
    }

    @CalledByNative
    private void notifyAdapterTurnedOn() {
        mAdapterOff = false;
        mItemChooserDialog.clear();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @CalledByNative
    public void notifyDiscoveryState(@DiscoveryMode int discoveryState) {
        switch (discoveryState) {
            case DiscoveryMode.DISCOVERY_FAILED_TO_START:
                // FAILED_TO_START might be caused by a missing Location
                // permission or by the Location service being turned off.
                // Check, and show a request if so.
                checkLocationServicesAndPermission();
                break;
            case DiscoveryMode.DISCOVERY_IDLE:
                mItemChooserDialog.setIdleState();
                break;
            default:
                // TODO(jyasskin): Report the new state to the user.
                break;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        void onDialogFinished(long nativeBluetoothChooserAndroid, int eventType, String deviceId);

        void restartSearch(long nativeBluetoothChooserAndroid);

        // Help links.
        void showBluetoothOverviewLink(long nativeBluetoothChooserAndroid);

        void showBluetoothAdapterOffLink(long nativeBluetoothChooserAndroid);

        void showNeedLocationPermissionLink(long nativeBluetoothChooserAndroid);
    }
}
