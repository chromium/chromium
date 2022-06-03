# ContentCapture Component

This directory contains ContentCapture’s renderer and browser parts. They work
with third_party/blink/renderer/core/content_capture to forward the message
from blink all the way to the ContentCaptureConsumer on the browser side.

## Classes Overview

ContentCaptureSender has an instance per RenderFrame, it receives the content
captured/removed messages from its [blink peer](/third_party/blink/renderer/core/content_capture/README.md) and sends them to
ContentCaptureReceiver on the browser side through the mojom interface.

ContentCaptureReceiver has an instance per RenderFrameHost, it gets the message
from ContentCaptureSender, then forwards it to ContentCaptureReceiverManager.
It also generates frame ID which is unique through the browser’s lifecycle, the
ID is required even if the ContentCaptureReceiver hasn’t received any message
yet; beside receiving messages, ContentCaptureReceiver can also start or stop
capturing content on demand.

ContentCaptureReceiverManager has an instance per WebContents and is the base
class intended to be subclassed by the embedders. It receives messages from
ContentCaptureReceiver, then attaches the ContentCaptureSession which represents
its relationship with the main frame, and forwards to the embedders.
ContentCaptureReceiverManager also works with ContentCaptureController to decide
whether to start or stop the content capture on navigation starts.

ContentCaptureReceiverManagerAndroid is an Android embedder which is used to
forward the messages to Java side.

ContentCaptureConsumer is a Java interface via which the implementation receives
the message that was forwarded by the ContentCaptureReceiverManager.

ContentCaptureConsumerImpl is a ContentCaptureConsumer for Android platform
service, it converts the received messages to the Android platform format, and
sends them to the Android Framework though the different tasks.
