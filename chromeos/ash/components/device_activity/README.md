# chromeos/ash/components/device_activity

This directory contains the code required to send active device pings
(segmentable across various dimensions) in a privacy-compliant manner.

In order to report activity for a given window, the client deterministically
generates a fingerprint using a high entropy seed which is sent to the Google
servers at most once. These are used to determine the device active counts.

Googlers: See go/chromeos-data-pc
