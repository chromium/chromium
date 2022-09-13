// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package android.widget.directwriting;

import android.widget.directwriting.IDirectWritingServiceCallback;
import android.os.Bundle;
import android.graphics.Rect;

/**
 * Direct writing service Interface definition for Samsung Platform's stylus handwriting recognition
 * service. These are the service APIs exposed to use the platform's handwriting recognition service
 * so that recognized text can be committed to input fields. Data needed to achieve functionality of
 * each API needs to be bundled in the expected format with keys and values as defined here.
 */
interface IDirectWritingService {

    const int VERSION = 1;

    /**
    * Key for bundle parameter MotionEvent
    */
    const String KEY_BUNDLE_EVENT = "event";

    /**
    * Key for bundle parameter Rect bounds of Edit field.
    */
    const String KEY_BUNDLE_EDIT_RECT = "editRect";

    /**
    * Key for bundle parameter Flag if only edit field bounds have changed.
    */
    const String KEY_BUNDLE_EDIT_RECT_RELOCATED = "onlyRectChanged";

    /**
    * Key for bundle parameter Flag if check only EditText bindable
    */
    const String KEY_BUNDLE_CHECK_CAN_BIND = "onlyCheckCanBind";

    /**
    * Key for bundle parameter Rect of RootView
    */
    const String KEY_BUNDLE_ROOT_VIEW_RECT = "rootViewRect";

    /**
    * Key for bundle parameter Service host source
    */
    const String KEY_BUNDLE_SERVICE_HOST_SOURCE = "hostSource";

    /**
    * Value for bundle parameter Service host source : samsunginternet, webview, viewroot
    */
    const String VALUE_BUNDLE_SERVICE_HOST_SOURCE_SAMSUNG_INTERNET = "samsunginternet";
    const String VALUE_BUNDLE_SERVICE_HOST_SOURCE_WEBVIEW = "webview";
    const String VALUE_BUNDLE_SERVICE_HOST_SOURCE_VIEWROOT = "viewroot";

    /**
    * Value for registerCallback packageName suffix to distinguish Samsung Internet or web view
    */
    const String VALUE_SERVICE_HOST_SOURCE_INTERNET = "|samsunginternet";
    const String VALUE_SERVICE_HOST_SOURCE_WEBVIEW = "|webview";

    /**
    * Key hide delay for bundle parameter of getConfiguration()
    */
    const String KEY_BUNDLE_CONFIG_HIDE_DELAY = "hideDelay";
    /**
    * Key keep writing delay for bundle parameter of getConfiguration()
    */
    const String KEY_BUNDLE_CONFIG_KEEP_WRITING_DELAY = "keepWritingDelay";
    /**
    * Key max distance for bundle parameter of getConfiguration()
    */
    const String KEY_BUNDLE_CONFIG_MAX_DISTANCE = "maxDistance";
    /**
    * Key trigger vertical space for bundle parameter of getConfiguration()
    */
    const String KEY_BUNDLE_CONFIG_TRIGGER_VERTICAL_SPACE = "triggerVerticalSpace";
    /**
    * Key trigger horizontal space for bundle parameter of getConfiguration()
    */
    const String KEY_BUNDLE_CONFIG_TRIGGER_HORIZONTAL_SPACE_DEFAULT = "triggerHorizontalSpace";
    /**
    * Key trigger to show keyboard with app privae command exceptionally when pen used
    * for bundle parameter of getConfiguration()
    */
    const String KEY_BUNDLE_CONFIG_FORCE_SHOW_SIP_APP_PRIVATE_COMMAND_LIST = "forceShowSipAppPrivateCommandList";
    /**
    * Key rejection distance to show transient bar during writing
    * for bundle parameter of getConfiguration()
    */
    const String KEY_BUNDLE_CONFIG_TRANSIENT_BAR_REJECT_DISTANCE = "transientBarRejectDistance";

    /**
    * Gets version of Aidl
    *
    * @returns version IDirectWritingService.VERSION
    */
    int getVersion() = 0;

    /**
    * Registers callback for service
    *
    * @param callback is IDirectWritingServiceCallback
    * @param packageName is Package name of bounded context
    * @returns true if success
    */
    boolean registerCallback(IDirectWritingServiceCallback callback, String packageName) = 1;

    /**
    * Unregisters callback for service
    *
    * @param callback is IDirectWritingServiceCallback
    * @returns true if success
    */
    boolean unregisterCallback(IDirectWritingServiceCallback callback) = 2;

    /**
    * Gets package name of bounded process
    *
    * @returns package name of bounded process
    */
    String getPackageName() = 3;

    /**
    * When Stylus pen's Motion event is detected by direct writing trigger,
    * Calls service to start direct writing recognition
    *
    * @param bundle is KEY_BUNDLE_EVENT, KEY_BUNDLE_EDIT_RECT, KEY_BUNDLE_ROOT_VIEW_RECT
    *                  KEY_BUNDLE_SERVICE_HOST_SOURCE for browser
    * @returns true if recognition is started
    */
    boolean onStartRecognition(in Bundle bundle) = 10;

    /**
    * When Stylus pen's Motion event is not intended to drawing
    * (e.g. when it's small amount of move)
    * Calls service to stop direct writing recognition
    *
    * @param bundle is KEY_BUNDLE_EVENT, KEY_BUNDLE_EDIT_RECT, KEY_BUNDLE_ROOT_VIEW_RECT
    *                  KEY_BUNDLE_SERVICE_HOST_SOURCE for browser
    * @returns true if recognition is stopped
    */
    boolean onStopRecognition(in Bundle bundle) = 11;

    /**
    * When current editing element bounds are changed in view, notify the service.
    *
    * @param bundle is KEY_BUNDLE_EDIT_RECT, KEY_BUNDLE_ROOT_VIEW_RECT
    *                  KEY_BUNDLE_SERVICE_HOST_SOURCE for browser
    * @returns true if onBoundedEditTextChanged successful
    */
    boolean onBoundedEditTextChanged(in Bundle bundle) = 12;

    /**
    * When window lost focus, notify the service
    *
    * @param packageName which is currently bound to service.
    */
    void onWindowFocusLost(String packageName) = 13;

    /**
    * Dispatch Motion Event to service for drawing
    *
    * @param bundle is KEY_BUNDLE_EVENT, KEY_BUNDLE_ROOT_VIEW_RECT
    */
    void onDispatchEvent(in Bundle bundle) = 14;

    /**
    * Notify to Direct Writing service when current Editing element's paste popup is shown to hide
    * the direct writing service floating toolbar. This API can be used for any use case where we
    * wish to hide this toolbar ex: changed focus to another tab, tap with finger instead of stylus,
    * and so on.
    *
    * @param bundle is KEY_BUNDLE_SERVICE_HOST_SOURCE for browser
    */
    void onEditTextActionModeStarted(in Bundle bundle) = 15;

    /**
    * Gets configuration from Remote service after service is connected.
    *
    * @param bundle is for configuration
    *        KEY_BUNDLE_CONFIG_HIDE_DELAY,
    *        KEY_BUNDLE_CONFIG_KEEP_WRITING_DELAY,
    *        KEY_BUNDLE_CONFIG_MAX_DISTANCE,
    *        KEY_BUNDLE_CONFIG_TRIGGER_VERTICAL_SPACE
    *        KEY_BUNDLE_CONFIG_TRIGGER_HORIZONTAL_SPACE_DEFAULT
    */
    void getConfiguration(inout Bundle bundle) = 16;

    /**
    * Notify to Direct Writing service when Edit field updates ImeOptions
    *
    * @param imeOptions is ImeOptions
    */
    void onUpdateImeOptions(int imeOptions) = 17;

    // { Common Extra
    /**
    * Extra Command for future use.
    * Note: This API is unused by Chromium for now. It is defined as per aidl from Samsung platform.
    *
    * @param action is for future use
    * @param bundle is for future use
    */
    void onExtraCommand(String action, inout Bundle bundle) = 900;
    // Common Extra }

    // { TextView
    /**
    * TextView Extra Command for future use.
    * Note: This API is unused by Chromium for now. It is defined as per aidl from Samsung platform.
    *
    * @param action is for future use
    * @param bundle is for future use
    */
    void onTextViewExtraCommand(String action, inout Bundle bundle) = 901;
    // TextView }
}
