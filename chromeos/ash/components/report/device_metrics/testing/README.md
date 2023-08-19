# Add Proto Test Data

If you would like to update the \*_preserved_file.binarypb
protobuf representing the PrivateComputingClientRegressionTestData (used in unit testing),
you can add new test cases to \*_preserved_file.textpb.

You may then generate a new binarypb using the updated textpb.

```
cd ../chromium/src/chromeos/ash/components/report/testing/

# Command used to convert between proto types (i.e textproto to binarypb)
# gqui from textproto:<path_to_textpb> <path_to_proto_message> --outfile=rawproto:<path_to_binarypb_destination>
gqui from textproto:<PREFIX>_preserved_file.textpb proto \
  ~/chromiumos/src/platform2/system_api/dbus/private_computing/private_computing_service.proto:private_computing.PrivateComputingClientRegressionTestData \
  --outfile=rawproto:<PREFIX>_preserved_file.binarypb
```

After it has been successfully generated, make sure you update the unit tests.
You can then upload the new textpb and binarypb to Gerrit.


```
# Build the chromeos unittests target.
autoninja -C out/Default chromeos_unittests

# Run the ../report/ unit tests.
./out/Default/chromeos_unittests --log-level=0 --enable-logging=stderr \
  --gtest_filter="*DeviceActi*:*UseCase*"
```
