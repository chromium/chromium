# Page Info Component

This directory contains the source code for the **Page Info** component in Chromium. This component provides the necessary APIs for showing the **Page Info UI**, which is accessed by clicking on the security status icon in the omnibox and related toolbars.

---

## Purpose

The **Page Info** component is responsible for:

* Displaying information about a website's permissions, connection state, and identity.
* Allowing users to view and change permissions for a specific website.
* Providing a UI for managing site-specific data, such as cookies and site data.

---

## Desktop UI Implementation

The desktop implementation of the Page Info UI is built using the **Views** framework, which is Chromium's cross-platform UI toolkit. The main entry point for the desktop UI is the `PageInfoBubbleView` class, which is a `BubbleDialogDelegateView` that displays the Page Info content in a bubble anchored to the omnibox.

### Key Files:

* `chrome/browser/ui/views/page_info/page_info_bubble_view.h / .cc`: Defines the `PageInfoBubbleView` class, which is the main view for the Page Info bubble on desktop. It is responsible for creating and managing the various sections of the bubble, such as permissions, site data, and connection information.
* `chrome/browser/ui/views/page_info/page_info_bubble_view_base.h / .cc`: A base class for the Page Info bubble, containing common logic shared across different bubble types.
* `chrome/browser/ui/views/page_info/permission_selector_row.h / .cc`: Represents a single row in the permissions section of the Page Info bubble, allowing users to view and change a specific permission.
* `chrome/browser/ui/views/page_info/chosen_object_view.h / .cc`: Represents a view for a chosen object, such as a USB device, in the Page Info bubble.

### How it Works

When the user clicks the lock icon in the omnibox, a `PageInfoBubbleView` is created and shown. This view uses the `PageInfo` object to get the necessary data about the current page and then populates its child views with this information. The user can interact with the controls in the bubble to change permissions, and these changes are then propagated back to the `PageInfo` object to be applied.

---

## Android Implementation

The Android implementation of the Page Info UI is located in the `components/page_info/android` directory and is written in Java. It uses Android's native UI components to present the same information as the desktop version.

### Key Files:

* `PageInfoController.java`: This is the main controller for the Page Info UI on Android. It's responsible for creating and showing the Page Info dialog, gathering the data from the `PageInfo` C++ backend, and coordinating the different parts of the UI.
* `PageInfoDialog.java`: This class represents the dialog that contains the Page Info view. It can be displayed as a sheet or a standard modal dialog, depending on the screen size.
* `PageInfoView.java`: This is the main view that holds all the UI elements of the Page Info dialog.
* `PageInfoPermissionsController.java`: This class manages the permissions section of the Page Info UI, displaying the list of permissions and handling user interactions.
* `PageInfoConnectionSecurityController.java`: This class is responsible for the connection security section of the UI, displaying information about the website's certificate and connection.

### How it Works

On Android, when a user taps the lock icon, the `PageInfoController` is created. It then creates a `PageInfoDialog` to display the UI. The `PageInfoController` communicates with the native `PageInfo` C++ code to get all the necessary information about the current website. This information is then used to populate the various views within the `PageInfoDialog`, such as the permissions list and the connection security details. User interactions, like changing a permission, are handled by the respective controllers, which then update the backend.
