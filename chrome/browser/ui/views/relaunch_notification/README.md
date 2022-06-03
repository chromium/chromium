# Relaunch Notifications Overview

This document is an overview of relaunch notifications concepts, terms, and
architecture.

## Key Classes

### RelaunchNotificationController

An instance of class RelaunchNotificationController observes `UpgradeDetector`
and controls the notifications shown to the user according to the
RelaunchNotification and RelaunchNotificationPeriod policies settings.
An instance of this class is owned by `ChromeBrowserMainExtraPartsViews`, and
only one instance is created per browser.

### RelaunchNotificationControllerPlatformImpl

An instance of this class represents the implementation of
RelaunchNotificationController according to specific platforms; Chrome OS,
Desktop.
The relaunch notification on Chrome OS is used to relaunch the whole operating
system to apply updates, but on other platforms is used to only relaunch the
browser to update it.

### RelaunchRequiredTimer

An instance of this class is responsible for refreshing the title for relaunch
notification in case `RelaunchNotification` policy is set to required.
The notification title needs to be refreshed as it displays the remaining time
for the relaunch.

### **Desktop Chrome**

#### RelaunchRecommendedBubbleView

If the policy setting is recommended, then  an instance of this class manages
the relaunch recommended notification bubble on desktop Chrome. The remaining
time until the next recommended relaunch is displayed in the title.

#### RelaunchRecommendedTimer

An instance of this class is responsible for refreshing the title for
RelaunchRecommendedBubbleView on desktop Chrome.

#### RelaunchRequiredDialogView

If the policy setting is required, then an instance of this class manages the
relaunch required dialog on desktop Chrome. The remaining time until the next
required relaunch is displayed in the title.

### **Chrome OS**

The relaunch notifications on Chrome OS uses `SystemTray` notifications, and
uses `RelaunchRequiredTimer` to update the title in case the
`RelaunchNotification` policy is set to required.
