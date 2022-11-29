# chromeos/ash/components/device_activity

This directory contains the code required to send active device pings
(segmentable across various dimensions) in a privacy-compliant manner.

In order to report activity for a given window, the client deterministically
generates a fingerprint using a high entropy seed which is sent to the Google
servers at most once. These are used to determine the device active counts.

Googlers: See go/chromeos-data-pc


## Testing

If you would like to update the private_computing_service_test_data.binarypb
protobuf representing the PrivateComputingClientRegressionTestData (used in unit testing),
you can add new test cases to private_computing_service_test_data.textpb.

You may then generate a new binarypb using the updated textpb.

```
cd ../chromium/src/chromeos/ash/components/device_activity/testing/

# Command used to convert between proto types (i.e textproto to binarypb)
# gqui from textproto:<path_to_textpb> <path_to_proto_message> --outfile=rawproto:<path_to_binarypb_destination>
gqui from textproto:private_computing_service_test_data.textpb proto \
  ~/chromiumos/src/platform2/system_api/dbus/private_computing/private_computing_service.proto:private_computing.PrivateComputingClientRegressionTestData \
  --outfile=rawproto:private_computing_service_test_data.binarypb
```

After it has been successfully generated, make sure you update the unit tests.
You can then upload the new textpb and binarypb to Gerrit.


```
# Build the chromeos unittests target.
autoninja -C out/Default chromeos_unittests

# Run the device_activity/ unit tests.
./out/Default/chromeos_unittests --log-level=0 --enable-logging=stderr \
  --gtest_filter="*DeviceActi*:*UseCase*"
```

