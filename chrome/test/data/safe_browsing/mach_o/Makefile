# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This must match the commonName in codesign.cfg.
KEYCHAIN_IDENTITY=untrusted@goat.local

# Functions to add and remove codesigning identity to user's keychain. These
# are necessary since the codesign utility no longer supports the -k option,
# which reads the identity from a file.
pre-build = security import codesign.key && security import codesign.crt
post-build = security delete-identity -c untrusted@goat.local

executable32: src.c
	clang -m32 -o $@ $^

executable64: src.c
	clang -m64 -o $@ $^

executablefat: executable32 executable64
	lipo -create -output $@ $^

lib32.dylib: src.c
	clang -m32 -shared -o $@ $^

lib64.dylib: src.c
	clang -m64 -shared -o $@ $^

libfat.dylib: lib64.dylib lib32.dylib
	lipo -create -output $@ $^

codesign.key:
	openssl genrsa -out $@ 2048

codesign.csr: codesign.key codesign.cfg
	openssl req -new -key $< -out $@ -config codesign.cfg

codesign.crt: codesign.csr codesign.key codesign.cfg
	openssl x509 -req -signkey codesign.key -sha256 -days 9999 \
		-extfile codesign.cfg -extensions req_attrs -in $< -out $@

signedexecutable32: executable32 codesign.crt
	$(call pre-build)
	cp $< $@
	codesign -s $(KEYCHAIN_IDENTITY) $@
	$(call post-build)

libsigned64.dylib: lib64.dylib codesign.crt
	$(call pre-build)
	cp $< $@
	codesign -s $(KEYCHAIN_IDENTITY) $@
	$(call post-build)

signedexecutablefat: executablefat codesign.crt
	$(call pre-build)
	cp $< $@
	codesign -s $(KEYCHAIN_IDENTITY) $@ --all-architectures
	$(call post-build)

signed-archive.dmg: test-bundle.app codesign.crt
	$(call pre-build)
	hdiutil create -srcfolder test-bundle.app -format UDZO -layout \
		SPUD -volname "Signed Archive" -ov $@
	codesign -s $(KEYCHAIN_IDENTITY) $@
	$(call post-build)

zipped-app-two-executables-one-signed.zip: executablefat signedexecutablefat
	ditto base-bundle.app app-with-executables.app
	ditto $< app-with-executables.app/Contents/MacOS/
	ditto $(word 2,$^) app-with-executables.app/Contents/MacOS/
	zip -r $@ app-with-executables.app
	rm -r app-with-executables.app

.PHONY: shell-script.app
shell-script.app:
	$(call pre-build)
	ditto base-bundle.app $@
	mkdir $@/Contents/MacOS || true
	echo 'echo "Hello world"' > $@/Contents/MacOS/test-bundle
	codesign -f -s $(KEYCHAIN_IDENTITY) $@
	$(call post-build)

.PHONY: test-bundle.app
test-bundle.app: signedexecutablefat libsigned64.dylib executable32
	$(call pre-build)
	ditto base-bundle.app $@
	ditto $< $@/Contents/MacOS/test-bundle
	ditto $(word 2,$^) $@/Contents/Frameworks/$(word 2,$^)
	ditto $(word 3,$^) $@/Contents/Resources/$(word 3,$^)
	codesign -f -s $(KEYCHAIN_IDENTITY) $@ --all-architectures \
		--resource-rules ResourceRules
	$(call post-build)

.PHONY: modified-bundle.app
modified-bundle.app: test-bundle.app lib32.dylib executable64
	$(call pre-build)
	ditto $< $@
	echo "<xml/>" > $@/Contents/Resources/Base.lproj/InfoPlist.strings
	codesign -f -s $(KEYCHAIN_IDENTITY) $@ --all-architectures \
		--resource-rules ResourceRules
	echo "BAD" > $@/Contents/Resources/Base.lproj/InfoPlist.strings
	touch $@/Contents/Resources/codesign.cfg
	ditto $(word 2,$^) $@/Contents/Frameworks/libsigned64.dylib
	ditto $(word 3,$^) $@/Contents/Resources/executable32
	echo "foo" >> $@/Contents/Resources/Base.lproj/MainMenu.nib
	codesign -f -s $(KEYCHAIN_IDENTITY) \
		$@/Contents/Resources/Base.lproj/MainMenu.nib
	$(call post-build)

.PHONY: modified-bundle-and-exec.app
modified-bundle-and-exec.app: test-bundle.app lib32.dylib executable64
	ditto $< $@
	touch $@/Contents/Resources/codesign.cfg
	ditto $(word 2,$^) $@/Contents/Frameworks/libsigned64.dylib
	ditto $(word 3,$^) $@/Contents/Resources/executable32
	printf '\x31' | dd bs=1 seek=8097 count=1 conv=notrunc \
	of=$@/Contents/MacOS/test-bundle

.PHONY: modified-main-exec32.app
modified-main-exec32.app: test-bundle.app
	ditto $< $@
	printf '\x31' | dd bs=1 seek=8097 count=1 conv=notrunc \
										 of=$@/Contents/MacOS/test-bundle

.PHONY: modified-main-exec64.app
modified-main-exec64.app: test-bundle.app
	ditto $< $@
	printf '\x31' | dd bs=1 seek=24448 count=1 conv=notrunc \
	of=$@/Contents/MacOS/test-bundle

.PHONY: modified-localization.app
modified-localization.app: test-bundle.app
	$(call pre-build)
	ditto $< $@
	echo "<xml/>" > $@/Contents/Resources/Base.lproj/InfoPlist.strings
	codesign -f -s $(KEYCHAIN_IDENTITY) $@ --all-architectures \
		--resource-rules ResourceRules
	echo "CORRUPT" > $@/Contents/Resources/Base.lproj/InfoPlist.strings
	$(call post-build)
