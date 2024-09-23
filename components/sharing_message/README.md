This directory contains components shared across platforms for
features for sharing data between one user's Chrome instances:
click-to-call, shared clipboard, send-tab-to-self push notifications
and so on. Social features for sharing between users go instead in
//chrome/browser/share. The features in this directory are backed
by either an [FCM] (Firebase Messaging) service or a [Chime] service
and can optionally use [VAPID].

[FCM]: https://firebase.google.com/docs/cloud-messaging
[VAPID]: https://datatracker.ietf.org/doc/html/draft-thomson-webpush-vapid-02
[Chime]: https://g3doc.corp.google.com/notifications/g3doc/index.md?cl=head
