// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import com.google.auto.service.AutoService;
import com.google.common.base.CaseFormat;
import com.google.common.collect.ImmutableSet;
import com.squareup.javapoet.ClassName;
import com.squareup.javapoet.FieldSpec;
import com.squareup.javapoet.JavaFile;
import com.squareup.javapoet.MethodSpec;
import com.squareup.javapoet.ParameterizedTypeName;
import com.squareup.javapoet.TypeName;
import com.squareup.javapoet.TypeSpec;

import java.util.Set;

import javax.annotation.processing.AbstractProcessor;
import javax.annotation.processing.Processor;
import javax.annotation.processing.RoundEnvironment;
import javax.lang.model.SourceVersion;
import javax.lang.model.element.Element;
import javax.lang.model.element.ElementKind;
import javax.lang.model.element.Modifier;
import javax.lang.model.element.PackageElement;
import javax.lang.model.element.TypeElement;
import javax.tools.Diagnostic;

/** Generates module classes for {@link ModuleInterface} annotations. */
@AutoService(Processor.class)
public class ModuleInterfaceProcessor extends AbstractProcessor {
    private static final Class<ModuleInterface> MODULE_INTERFACE_CLASS = ModuleInterface.class;

    @Override
    public Set<String> getSupportedAnnotationTypes() {
        return ImmutableSet.of(MODULE_INTERFACE_CLASS.getCanonicalName());
    }

    @Override
    public SourceVersion getSupportedSourceVersion() {
        return SourceVersion.latestSupported();
    }

    @Override
    public boolean process(
            Set<? extends TypeElement> annotations, RoundEnvironment roundEnvironment) {
        // Do nothing on an empty round.
        if (annotations.isEmpty()) {
            return true;
        }

        for (Element e : roundEnvironment.getElementsAnnotatedWith(MODULE_INTERFACE_CLASS)) {
            // @ModuleInterface can only annotate types so this is safe.
            TypeElement type = (TypeElement) e;
            ModuleInterface annotation = e.getAnnotation(ModuleInterface.class);
            TypeSpec moduleClass =
                    createModuleClassSpec(annotation.module(), type, annotation.impl());

            JavaFile file = JavaFile.builder(getPackageName(type), moduleClass).build();
            try {
                file.writeTo(processingEnv.getFiler());
            } catch (Exception ex) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, ex.getMessage());
            }
        }

        return true;
    }

    private TypeSpec createModuleClassSpec(
            String moduleName, TypeElement moduleInterface, String implClassName) {
        ClassName fooModuleClassName = ClassName.get(getPackageName(moduleInterface),
                CaseFormat.LOWER_UNDERSCORE.to(CaseFormat.UPPER_CAMEL, moduleName) + "Module");
        TypeName interfaceClassName = ClassName.get(moduleInterface);
        TypeName moduleClassName = ParameterizedTypeName.get(
                ClassName.get("org.chromium.components.module_installer.builder", "Module"),
                interfaceClassName);
        TypeName listenerInterface =
                ClassName.get("org.chromium.components.module_installer.engine", "InstallListener");
        TypeName installEngineInterface =
                ClassName.get("org.chromium.components.module_installer.engine", "InstallEngine");

        FieldSpec module = FieldSpec.builder(moduleClassName, "sModule")
                                   .addModifiers(Modifier.PRIVATE, Modifier.STATIC, Modifier.FINAL)
                                   .initializer("new $T($S, $T.class, $S)", moduleClassName,
                                           moduleName, moduleInterface, implClassName)
                                   .build();

        MethodSpec isInstalled = MethodSpec.methodBuilder("isInstalled")
                                         .returns(TypeName.BOOLEAN)
                                         .addModifiers(Modifier.PUBLIC, Modifier.STATIC)
                                         .addStatement("return sModule.isInstalled()")
                                         .build();

        MethodSpec install = MethodSpec.methodBuilder("install")
                                     .returns(TypeName.VOID)
                                     .addModifiers(Modifier.PUBLIC, Modifier.STATIC)
                                     .addParameter(listenerInterface, "listener")
                                     .addStatement("sModule.install(listener)")
                                     .build();

        MethodSpec installDeferred = MethodSpec.methodBuilder("installDeferred")
                                             .returns(TypeName.VOID)
                                             .addModifiers(Modifier.PUBLIC, Modifier.STATIC)
                                             .addStatement("sModule.installDeferred()")
                                             .build();

        MethodSpec ensureNativeLoaded = MethodSpec.methodBuilder("ensureNativeLoaded")
                                                .returns(TypeName.VOID)
                                                .addModifiers(Modifier.PUBLIC, Modifier.STATIC)
                                                .addStatement("sModule.ensureNativeLoaded()")
                                                .build();

        MethodSpec getImpl = MethodSpec.methodBuilder("getImpl")
                                     .returns(interfaceClassName)
                                     .addModifiers(Modifier.PUBLIC, Modifier.STATIC)
                                     .addStatement("return sModule.getImpl()")
                                     .build();

        MethodSpec getInstallEngine = MethodSpec.methodBuilder("getInstallEngine")
                                              .returns(installEngineInterface)
                                              .addModifiers(Modifier.PUBLIC, Modifier.STATIC)
                                              .addStatement("return sModule.getInstallEngine()")
                                              .build();

        MethodSpec setInstallEngine = MethodSpec.methodBuilder("setInstallEngine")
                                              .returns(TypeName.VOID)
                                              .addModifiers(Modifier.PUBLIC, Modifier.STATIC)
                                              .addParameter(installEngineInterface, "engine")
                                              .addStatement("sModule.setInstallEngine(engine)")
                                              .build();

        MethodSpec constructor =
                MethodSpec.constructorBuilder().addModifiers(Modifier.PRIVATE).build();

        return TypeSpec.classBuilder(fooModuleClassName)
                .addModifiers(Modifier.PUBLIC)
                .addField(module)
                .addMethod(constructor)
                .addMethod(isInstalled)
                .addMethod(install)
                .addMethod(installDeferred)
                .addMethod(ensureNativeLoaded)
                .addMethod(getImpl)
                .addMethod(getInstallEngine)
                .addMethod(setInstallEngine)
                .build();
    }

    private static String getPackageName(Element element) {
        while (element.getKind() != ElementKind.PACKAGE) {
            element = element.getEnclosingElement();
        }
        return ((PackageElement) element).getQualifiedName().toString();
    }
}
