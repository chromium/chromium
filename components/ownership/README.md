#Ownership

[TOC]

## Objective

The concept of ownership allows ChromeOS to decide who should have the full
control over the device, including changing settings that might affect all users
on the device.

## Terminology
- Owner: The entity that “owns” the device. This could be a user or a device
management organization. The owner possesses the owner private key.
- Owner private key: The private key used to sign device settings. Only the
owner has access to it.
- Owner public key: The public key used to verify device settings. All users on
the device have access to it.
- Owner key pair: The key pair consisting of owner private key and owner public
key.

## Consumer ownership

A ChromeOS device can be owned by a single user (in contrast to an
organization), in which case it is consumer owned.

The first user created on the device becomes the owner. ChromeOS generates an
owner key pair for the user and produces initial device settings. The device
settings mention the owner user as the author, they include the public part of
the owner key and they are signed by the private part of the owner key. Chrome
sends signed device settings to the session manager daemon for storing them on
disk. Session manager also stores the public owner key separately, so all users
can verify the signature on the device settings.

Later the owner user can produce, sign and store new device settings as long as
they have the owner key. And other users can read and verify them, but they
cannot change them.

If the public part of the owner key gets lost or corrupted, it can be restored
by the session manager from the device settings. If the private part of the key
is lost and the device settings claim that the current user is the owner, it is
allowed to generate a new owner key and store new device settings signed with
it. As a last resort the local state preferences also store which user is the
owner.

## Enterprise ownership

A ChromeOS device can be owned by an organization, in which case it is
enterprise managed.

To initiate this mode the device needs to be enterprise enrolled on the OOBE
(out-of-the-box experience) screen or automatically. The owner key pair is owned
by the management server and the device only receives the public part of the
owner key together with device policies from it. Session manager processes
device policies from the management server in the same way as device settings
on consumer devices.

The management server has an ability to rotate the owner key.
