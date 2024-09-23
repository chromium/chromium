# Navigation interception

This component is for navigation interception on the Android platform.
This component directory provides an interface for intercepting these
navigations, however it does not implement any other behavior. It's up to the
content embedder (ex. //chrome folder) to choose what to do once the navigation
is intercepted.

An example of something the content embedder may choose to do might be to create
an [`Intent`] to another application. An example of this is when a user clicks
on a link to a YouTube video: in this scenario, the browser may prefer to send
an `Intent` to the Android YouTube app (if it's installed on the user's device),
so that the video can play within that app instead of in the browser.

[`Intent`]: https://developer.android.com/reference/android/content/Intent
