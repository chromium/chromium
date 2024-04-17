// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/graphics/gles2_starboard.h"

#include <starboard/gles.h>

extern "C" {

// gl2.h

GL_APICALL void GL_APIENTRY Sb_glActiveTexture(GLenum texture) {
  return SbGetGlesInterface()->glActiveTexture(texture);
}

GL_APICALL void GL_APIENTRY Sb_glAttachShader(GLuint program, GLuint shader) {
  return SbGetGlesInterface()->glAttachShader(program, shader);
}

GL_APICALL void GL_APIENTRY Sb_glBindAttribLocation(GLuint program,
                                                    GLuint index,
                                                    const GLchar* name) {
  return SbGetGlesInterface()->glBindAttribLocation(program, index, name);
}

GL_APICALL void GL_APIENTRY Sb_glBindBuffer(GLenum target, GLuint buffer) {
  return SbGetGlesInterface()->glBindBuffer(target, buffer);
}

GL_APICALL void GL_APIENTRY Sb_glBindFramebuffer(GLenum target,
                                                 GLuint framebuffer) {
  return SbGetGlesInterface()->glBindFramebuffer(target, framebuffer);
}

GL_APICALL void GL_APIENTRY Sb_glBindRenderbuffer(GLenum target,
                                                  GLuint renderbuffer) {
  return SbGetGlesInterface()->glBindRenderbuffer(target, renderbuffer);
}

GL_APICALL void GL_APIENTRY Sb_glBindTexture(GLenum target, GLuint texture) {
  return SbGetGlesInterface()->glBindTexture(target, texture);
}

GL_APICALL void GL_APIENTRY Sb_glBlendColor(GLfloat red,
                                            GLfloat green,
                                            GLfloat blue,
                                            GLfloat alpha) {
  return SbGetGlesInterface()->glBlendColor(red, green, blue, alpha);
}

GL_APICALL void GL_APIENTRY Sb_glBlendEquation(GLenum mode) {
  return SbGetGlesInterface()->glBlendEquation(mode);
}

GL_APICALL void GL_APIENTRY Sb_glBlendEquationSeparate(GLenum modeRGB,
                                                       GLenum modeAlpha) {
  return SbGetGlesInterface()->glBlendEquationSeparate(modeRGB, modeAlpha);
}

GL_APICALL void GL_APIENTRY Sb_glBlendFunc(GLenum sfactor, GLenum dfactor) {
  return SbGetGlesInterface()->glBlendFunc(sfactor, dfactor);
}

GL_APICALL void GL_APIENTRY Sb_glBlendFuncSeparate(GLenum sfactorRGB,
                                                   GLenum dfactorRGB,
                                                   GLenum sfactorAlpha,
                                                   GLenum dfactorAlpha) {
  return SbGetGlesInterface()->glBlendFuncSeparate(sfactorRGB, dfactorRGB,
                                                   sfactorAlpha, dfactorAlpha);
}

GL_APICALL void GL_APIENTRY Sb_glBufferData(GLenum target,
                                            GLsizeiptr size,
                                            const void* data,
                                            GLenum usage) {
  return SbGetGlesInterface()->glBufferData(target, size, data, usage);
}

GL_APICALL void GL_APIENTRY Sb_glBufferSubData(GLenum target,
                                               GLintptr offset,
                                               GLsizeiptr size,
                                               const void* data) {
  return SbGetGlesInterface()->glBufferSubData(target, offset, size, data);
}

GL_APICALL GLenum GL_APIENTRY Sb_glCheckFramebufferStatus(GLenum target) {
  return SbGetGlesInterface()->glCheckFramebufferStatus(target);
}

GL_APICALL void GL_APIENTRY Sb_glClear(GLbitfield mask) {
  return SbGetGlesInterface()->glClear(mask);
}

GL_APICALL void GL_APIENTRY Sb_glClearColor(GLfloat red,
                                            GLfloat green,
                                            GLfloat blue,
                                            GLfloat alpha) {
  // TODO(rknichols):
  // When casting, the UI overlay is rendered on top of the video frame.
  // We must use punch through to display the video frame underneath, thus
  // the alpha when displaying videos must be 0.f.
  // However, sourcing the location in chromium's code that sets us to (0,0,0,1)
  // is proving difficult. the last breadcrumb has been:
  // `cc/layers/recording_source.cc:DetermineIfSolidColor`
  // if `solid_color_` is set at the end of the function, to 0 then we observe
  // the expected output. Where the originating `black` layer comes from is
  // unclear.
  //
  // Remove the force set of alpha to 0.f when the above comment has been
  // resolved.
  return SbGetGlesInterface()->glClearColor(red, green, blue, 0.f);
}

GL_APICALL void GL_APIENTRY Sb_glClearDepthf(GLfloat d) {
  return SbGetGlesInterface()->glClearDepthf(d);
}

GL_APICALL void GL_APIENTRY Sb_glClearStencil(GLint s) {
  return SbGetGlesInterface()->glClearStencil(s);
}

GL_APICALL void GL_APIENTRY Sb_glColorMask(GLboolean red,
                                           GLboolean green,
                                           GLboolean blue,
                                           GLboolean alpha) {
  return SbGetGlesInterface()->glColorMask(red, green, blue, alpha);
}

GL_APICALL void GL_APIENTRY Sb_glCompileShader(GLuint shader) {
  return SbGetGlesInterface()->glCompileShader(shader);
}

GL_APICALL void GL_APIENTRY Sb_glCompressedTexImage2D(GLenum target,
                                                      GLint level,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      GLint border,
                                                      GLsizei imageSize,
                                                      const void* data) {
  return SbGetGlesInterface()->glCompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
}

GL_APICALL void GL_APIENTRY Sb_glCompressedTexSubImage2D(GLenum target,
                                                         GLint level,
                                                         GLint xoffset,
                                                         GLint yoffset,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLenum format,
                                                         GLsizei imageSize,
                                                         const void* data) {
  return SbGetGlesInterface()->glCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

GL_APICALL void GL_APIENTRY Sb_glCopyTexImage2D(GLenum target,
                                                GLint level,
                                                GLenum internalformat,
                                                GLint x,
                                                GLint y,
                                                GLsizei width,
                                                GLsizei height,
                                                GLint border) {
  return SbGetGlesInterface()->glCopyTexImage2D(target, level, internalformat,
                                                x, y, width, height, border);
}

GL_APICALL void GL_APIENTRY Sb_glCopyTexSubImage2D(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLint x,
                                                   GLint y,
                                                   GLsizei width,
                                                   GLsizei height) {
  return SbGetGlesInterface()->glCopyTexSubImage2D(
      target, level, xoffset, yoffset, x, y, width, height);
}

GL_APICALL GLuint GL_APIENTRY Sb_glCreateProgram(void) {
  return SbGetGlesInterface()->glCreateProgram();
}

GL_APICALL GLuint GL_APIENTRY Sb_glCreateShader(GLenum type) {
  return SbGetGlesInterface()->glCreateShader(type);
}

GL_APICALL void GL_APIENTRY Sb_glCullFace(GLenum mode) {
  return SbGetGlesInterface()->glCullFace(mode);
}

GL_APICALL void GL_APIENTRY Sb_glDeleteBuffers(GLsizei n,
                                               const GLuint* buffers) {
  return SbGetGlesInterface()->glDeleteBuffers(n, buffers);
}

GL_APICALL void GL_APIENTRY
Sb_glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  return SbGetGlesInterface()->glDeleteFramebuffers(n, framebuffers);
}

GL_APICALL void GL_APIENTRY Sb_glDeleteProgram(GLuint program) {
  return SbGetGlesInterface()->glDeleteProgram(program);
}

GL_APICALL void GL_APIENTRY
Sb_glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  return SbGetGlesInterface()->glDeleteRenderbuffers(n, renderbuffers);
}

GL_APICALL void GL_APIENTRY Sb_glDeleteShader(GLuint shader) {
  return SbGetGlesInterface()->glDeleteShader(shader);
}

GL_APICALL void GL_APIENTRY Sb_glDeleteTextures(GLsizei n,
                                                const GLuint* textures) {
  return SbGetGlesInterface()->glDeleteTextures(n, textures);
}

GL_APICALL void GL_APIENTRY Sb_glDepthFunc(GLenum func) {
  return SbGetGlesInterface()->glDepthFunc(func);
}

GL_APICALL void GL_APIENTRY Sb_glDepthMask(GLboolean flag) {
  return SbGetGlesInterface()->glDepthMask(flag);
}

GL_APICALL void GL_APIENTRY Sb_glDepthRangef(GLfloat n, GLfloat f) {
  return SbGetGlesInterface()->glDepthRangef(n, f);
}

GL_APICALL void GL_APIENTRY Sb_glDetachShader(GLuint program, GLuint shader) {
  return SbGetGlesInterface()->glDetachShader(program, shader);
}

GL_APICALL void GL_APIENTRY Sb_glDisable(GLenum cap) {
  return SbGetGlesInterface()->glDisable(cap);
}

GL_APICALL void GL_APIENTRY Sb_glDisableVertexAttribArray(GLuint index) {
  return SbGetGlesInterface()->glDisableVertexAttribArray(index);
}

GL_APICALL void GL_APIENTRY Sb_glDrawArrays(GLenum mode,
                                            GLint first,
                                            GLsizei count) {
  return SbGetGlesInterface()->glDrawArrays(mode, first, count);
}

GL_APICALL void GL_APIENTRY Sb_glDrawElements(GLenum mode,
                                              GLsizei count,
                                              GLenum type,
                                              const void* indices) {
  return SbGetGlesInterface()->glDrawElements(mode, count, type, indices);
}

GL_APICALL void GL_APIENTRY Sb_glEnable(GLenum cap) {
  return SbGetGlesInterface()->glEnable(cap);
}

GL_APICALL void GL_APIENTRY Sb_glEnableVertexAttribArray(GLuint index) {
  return SbGetGlesInterface()->glEnableVertexAttribArray(index);
}

GL_APICALL void GL_APIENTRY Sb_glFinish(void) {
  return SbGetGlesInterface()->glFinish();
}

GL_APICALL void GL_APIENTRY Sb_glFlush(void) {
  return SbGetGlesInterface()->glFlush();
}

GL_APICALL void GL_APIENTRY
Sb_glFramebufferRenderbuffer(GLenum target,
                             GLenum attachment,
                             GLenum renderbuffertarget,
                             GLuint renderbuffer) {
  return SbGetGlesInterface()->glFramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}

GL_APICALL void GL_APIENTRY Sb_glFramebufferTexture2D(GLenum target,
                                                      GLenum attachment,
                                                      GLenum textarget,
                                                      GLuint texture,
                                                      GLint level) {
  return SbGetGlesInterface()->glFramebufferTexture2D(
      target, attachment, textarget, texture, level);
}

GL_APICALL void GL_APIENTRY Sb_glFrontFace(GLenum mode) {
  return SbGetGlesInterface()->glFrontFace(mode);
}

GL_APICALL void GL_APIENTRY Sb_glGenBuffers(GLsizei n, GLuint* buffers) {
  return SbGetGlesInterface()->glGenBuffers(n, buffers);
}

GL_APICALL void GL_APIENTRY Sb_glGenerateMipmap(GLenum target) {
  return SbGetGlesInterface()->glGenerateMipmap(target);
}

GL_APICALL void GL_APIENTRY Sb_glGenFramebuffers(GLsizei n,
                                                 GLuint* framebuffers) {
  return SbGetGlesInterface()->glGenFramebuffers(n, framebuffers);
}

GL_APICALL void GL_APIENTRY Sb_glGenRenderbuffers(GLsizei n,
                                                  GLuint* renderbuffers) {
  return SbGetGlesInterface()->glGenRenderbuffers(n, renderbuffers);
}

GL_APICALL void GL_APIENTRY Sb_glGenTextures(GLsizei n, GLuint* textures) {
  return SbGetGlesInterface()->glGenTextures(n, textures);
}

GL_APICALL void GL_APIENTRY Sb_glGetActiveAttrib(GLuint program,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* size,
                                                 GLenum* type,
                                                 GLchar* name) {
  return SbGetGlesInterface()->glGetActiveAttrib(program, index, bufSize,
                                                 length, size, type, name);
}

GL_APICALL void GL_APIENTRY Sb_glGetActiveUniform(GLuint program,
                                                  GLuint index,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* size,
                                                  GLenum* type,
                                                  GLchar* name) {
  return SbGetGlesInterface()->glGetActiveUniform(program, index, bufSize,
                                                  length, size, type, name);
}

GL_APICALL void GL_APIENTRY Sb_glGetAttachedShaders(GLuint program,
                                                    GLsizei maxCount,
                                                    GLsizei* count,
                                                    GLuint* shaders) {
  return SbGetGlesInterface()->glGetAttachedShaders(program, maxCount, count,
                                                    shaders);
}

GL_APICALL GLint GL_APIENTRY Sb_glGetAttribLocation(GLuint program,
                                                    const GLchar* name) {
  return SbGetGlesInterface()->glGetAttribLocation(program, name);
}

GL_APICALL void GL_APIENTRY Sb_glGetBooleanv(GLenum pname, GLboolean* data) {
  return SbGetGlesInterface()->glGetBooleanv(pname, data);
}

GL_APICALL void GL_APIENTRY Sb_glGetBufferParameteriv(GLenum target,
                                                      GLenum pname,
                                                      GLint* params) {
  return SbGetGlesInterface()->glGetBufferParameteriv(target, pname, params);
}

GL_APICALL GLenum GL_APIENTRY Sb_glGetError(void) {
  return SbGetGlesInterface()->glGetError();
}

GL_APICALL void GL_APIENTRY Sb_glGetFloatv(GLenum pname, GLfloat* data) {
  return SbGetGlesInterface()->glGetFloatv(pname, data);
}

GL_APICALL void GL_APIENTRY
Sb_glGetFramebufferAttachmentParameteriv(GLenum target,
                                         GLenum attachment,
                                         GLenum pname,
                                         GLint* params) {
  return SbGetGlesInterface()->glGetFramebufferAttachmentParameteriv(
      target, attachment, pname, params);
}

GL_APICALL void GL_APIENTRY Sb_glGetIntegerv(GLenum pname, GLint* data) {
  return SbGetGlesInterface()->glGetIntegerv(pname, data);
}

GL_APICALL void GL_APIENTRY Sb_glGetProgramiv(GLuint program,
                                              GLenum pname,
                                              GLint* params) {
  return SbGetGlesInterface()->glGetProgramiv(program, pname, params);
}

GL_APICALL void GL_APIENTRY Sb_glGetProgramInfoLog(GLuint program,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLchar* infoLog) {
  return SbGetGlesInterface()->glGetProgramInfoLog(program, bufSize, length,
                                                   infoLog);
}

GL_APICALL void GL_APIENTRY Sb_glGetRenderbufferParameteriv(GLenum target,
                                                            GLenum pname,
                                                            GLint* params) {
  return SbGetGlesInterface()->glGetRenderbufferParameteriv(target, pname,
                                                            params);
}

GL_APICALL void GL_APIENTRY Sb_glGetShaderiv(GLuint shader,
                                             GLenum pname,
                                             GLint* params) {
  return SbGetGlesInterface()->glGetShaderiv(shader, pname, params);
}

GL_APICALL void GL_APIENTRY Sb_glGetShaderInfoLog(GLuint shader,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLchar* infoLog) {
  return SbGetGlesInterface()->glGetShaderInfoLog(shader, bufSize, length,
                                                  infoLog);
}

GL_APICALL void GL_APIENTRY Sb_glGetShaderPrecisionFormat(GLenum shadertype,
                                                          GLenum precisiontype,
                                                          GLint* range,
                                                          GLint* precision) {
  return SbGetGlesInterface()->glGetShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}

GL_APICALL void GL_APIENTRY Sb_glGetShaderSource(GLuint shader,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLchar* source) {
  return SbGetGlesInterface()->glGetShaderSource(shader, bufSize, length,
                                                 source);
}

GL_APICALL const GLubyte* GL_APIENTRY Sb_glGetString(GLenum name) {
  static const unsigned char opengl_es_2_str[] = "OpenGL ES 2.0";

  const GLubyte* result = SbGetGlesInterface()->glGetString(name);
  return (result && name == SB_GL_VERSION) ? opengl_es_2_str : result;
}

GL_APICALL void GL_APIENTRY Sb_glGetTexParameterfv(GLenum target,
                                                   GLenum pname,
                                                   GLfloat* params) {
  return SbGetGlesInterface()->glGetTexParameterfv(target, pname, params);
}

GL_APICALL void GL_APIENTRY Sb_glGetTexParameteriv(GLenum target,
                                                   GLenum pname,
                                                   GLint* params) {
  return SbGetGlesInterface()->glGetTexParameteriv(target, pname, params);
}

GL_APICALL void GL_APIENTRY Sb_glGetUniformfv(GLuint program,
                                              GLint location,
                                              GLfloat* params) {
  return SbGetGlesInterface()->glGetUniformfv(program, location, params);
}

GL_APICALL void GL_APIENTRY Sb_glGetUniformiv(GLuint program,
                                              GLint location,
                                              GLint* params) {
  return SbGetGlesInterface()->glGetUniformiv(program, location, params);
}

GL_APICALL GLint GL_APIENTRY Sb_glGetUniformLocation(GLuint program,
                                                     const GLchar* name) {
  return SbGetGlesInterface()->glGetUniformLocation(program, name);
}

GL_APICALL void GL_APIENTRY Sb_glGetVertexAttribfv(GLuint index,
                                                   GLenum pname,
                                                   GLfloat* params) {
  return SbGetGlesInterface()->glGetVertexAttribfv(index, pname, params);
}

GL_APICALL void GL_APIENTRY Sb_glGetVertexAttribiv(GLuint index,
                                                   GLenum pname,
                                                   GLint* params) {
  return SbGetGlesInterface()->glGetVertexAttribiv(index, pname, params);
}

GL_APICALL void GL_APIENTRY Sb_glGetVertexAttribPointerv(GLuint index,
                                                         GLenum pname,
                                                         void** pointer) {
  return SbGetGlesInterface()->glGetVertexAttribPointerv(index, pname, pointer);
}

GL_APICALL void GL_APIENTRY Sb_glHint(GLenum target, GLenum mode) {
  return SbGetGlesInterface()->glHint(target, mode);
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsBuffer(GLuint buffer) {
  return SbGetGlesInterface()->glIsBuffer(buffer);
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsEnabled(GLenum cap) {
  return SbGetGlesInterface()->glIsEnabled(cap);
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsFramebuffer(GLuint framebuffer) {
  return SbGetGlesInterface()->glIsFramebuffer(framebuffer);
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsProgram(GLuint program) {
  return SbGetGlesInterface()->glIsProgram(program);
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsRenderbuffer(GLuint renderbuffer) {
  return SbGetGlesInterface()->glIsRenderbuffer(renderbuffer);
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsShader(GLuint shader) {
  return SbGetGlesInterface()->glIsShader(shader);
}

GL_APICALL GLboolean GL_APIENTRY Sb_glIsTexture(GLuint texture) {
  return SbGetGlesInterface()->glIsTexture(texture);
}

GL_APICALL void GL_APIENTRY Sb_glLineWidth(GLfloat width) {
  return SbGetGlesInterface()->glLineWidth(width);
}

GL_APICALL void GL_APIENTRY Sb_glLinkProgram(GLuint program) {
  return SbGetGlesInterface()->glLinkProgram(program);
}

GL_APICALL void GL_APIENTRY Sb_glPixelStorei(GLenum pname, GLint param) {
  return SbGetGlesInterface()->glPixelStorei(pname, param);
}

GL_APICALL void GL_APIENTRY Sb_glPolygonOffset(GLfloat factor, GLfloat units) {
  return SbGetGlesInterface()->glPolygonOffset(factor, units);
}

GL_APICALL void GL_APIENTRY Sb_glReadPixels(GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            void* pixels) {
  return SbGetGlesInterface()->glReadPixels(x, y, width, height, format, type,
                                            pixels);
}

GL_APICALL void GL_APIENTRY Sb_glReleaseShaderCompiler(void) {
  return SbGetGlesInterface()->glReleaseShaderCompiler();
}

GL_APICALL void GL_APIENTRY Sb_glRenderbufferStorage(GLenum target,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height) {
  return SbGetGlesInterface()->glRenderbufferStorage(target, internalformat,
                                                     width, height);
}

GL_APICALL void GL_APIENTRY Sb_glSampleCoverage(GLfloat value,
                                                GLboolean invert) {
  return SbGetGlesInterface()->glSampleCoverage(value, invert);
}

GL_APICALL void GL_APIENTRY Sb_glScissor(GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height) {
  return SbGetGlesInterface()->glScissor(x, y, width, height);
}

GL_APICALL void GL_APIENTRY Sb_glShaderBinary(GLsizei count,
                                              const GLuint* shaders,
                                              GLenum binaryformat,
                                              const void* binary,
                                              GLsizei length) {
  return SbGetGlesInterface()->glShaderBinary(count, shaders, binaryformat,
                                              binary, length);
}

GL_APICALL void GL_APIENTRY Sb_glShaderSource(GLuint shader,
                                              GLsizei count,
                                              const GLchar* const* string,
                                              const GLint* length) {
  return SbGetGlesInterface()->glShaderSource(shader, count, string, length);
}

GL_APICALL void GL_APIENTRY Sb_glStencilFunc(GLenum func,
                                             GLint ref,
                                             GLuint mask) {
  return SbGetGlesInterface()->glStencilFunc(func, ref, mask);
}

GL_APICALL void GL_APIENTRY Sb_glStencilFuncSeparate(GLenum face,
                                                     GLenum func,
                                                     GLint ref,
                                                     GLuint mask) {
  return SbGetGlesInterface()->glStencilFuncSeparate(face, func, ref, mask);
}

GL_APICALL void GL_APIENTRY Sb_glStencilMask(GLuint mask) {
  return SbGetGlesInterface()->glStencilMask(mask);
}

GL_APICALL void GL_APIENTRY Sb_glStencilMaskSeparate(GLenum face, GLuint mask) {
  return SbGetGlesInterface()->glStencilMaskSeparate(face, mask);
}

GL_APICALL void GL_APIENTRY Sb_glStencilOp(GLenum fail,
                                           GLenum zfail,
                                           GLenum zpass) {
  return SbGetGlesInterface()->glStencilOp(fail, zfail, zpass);
}

GL_APICALL void GL_APIENTRY Sb_glStencilOpSeparate(GLenum face,
                                                   GLenum sfail,
                                                   GLenum dpfail,
                                                   GLenum dppass) {
  return SbGetGlesInterface()->glStencilOpSeparate(face, sfail, dpfail, dppass);
}

GL_APICALL void GL_APIENTRY Sb_glTexImage2D(GLenum target,
                                            GLint level,
                                            GLint internalformat,
                                            GLsizei width,
                                            GLsizei height,
                                            GLint border,
                                            GLenum format,
                                            GLenum type,
                                            const void* pixels) {
  return SbGetGlesInterface()->glTexImage2D(target, level, internalformat,
                                            width, height, border, format, type,
                                            pixels);
}

GL_APICALL void GL_APIENTRY Sb_glTexParameterf(GLenum target,
                                               GLenum pname,
                                               GLfloat param) {
  return SbGetGlesInterface()->glTexParameterf(target, pname, param);
}

GL_APICALL void GL_APIENTRY Sb_glTexParameterfv(GLenum target,
                                                GLenum pname,
                                                const GLfloat* params) {
  return SbGetGlesInterface()->glTexParameterfv(target, pname, params);
}

GL_APICALL void GL_APIENTRY Sb_glTexParameteri(GLenum target,
                                               GLenum pname,
                                               GLint param) {
  return SbGetGlesInterface()->glTexParameteri(target, pname, param);
}

GL_APICALL void GL_APIENTRY Sb_glTexParameteriv(GLenum target,
                                                GLenum pname,
                                                const GLint* params) {
  return SbGetGlesInterface()->glTexParameteriv(target, pname, params);
}

GL_APICALL void GL_APIENTRY Sb_glTexSubImage2D(GLenum target,
                                               GLint level,
                                               GLint xoffset,
                                               GLint yoffset,
                                               GLsizei width,
                                               GLsizei height,
                                               GLenum format,
                                               GLenum type,
                                               const void* pixels) {
  return SbGetGlesInterface()->glTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}

GL_APICALL void GL_APIENTRY Sb_glUniform1f(GLint location, GLfloat v0) {
  return SbGetGlesInterface()->glUniform1f(location, v0);
}

GL_APICALL void GL_APIENTRY Sb_glUniform1fv(GLint location,
                                            GLsizei count,
                                            const GLfloat* value) {
  return SbGetGlesInterface()->glUniform1fv(location, count, value);
}

GL_APICALL void GL_APIENTRY Sb_glUniform1i(GLint location, GLint v0) {
  return SbGetGlesInterface()->glUniform1i(location, v0);
}

GL_APICALL void GL_APIENTRY Sb_glUniform1iv(GLint location,
                                            GLsizei count,
                                            const GLint* value) {
  return SbGetGlesInterface()->glUniform1iv(location, count, value);
}

GL_APICALL void GL_APIENTRY Sb_glUniform2f(GLint location,
                                           GLfloat v0,
                                           GLfloat v1) {
  return SbGetGlesInterface()->glUniform2f(location, v0, v1);
}

GL_APICALL void GL_APIENTRY Sb_glUniform2fv(GLint location,
                                            GLsizei count,
                                            const GLfloat* value) {
  return SbGetGlesInterface()->glUniform2fv(location, count, value);
}

GL_APICALL void GL_APIENTRY Sb_glUniform2i(GLint location, GLint v0, GLint v1) {
  return SbGetGlesInterface()->glUniform2i(location, v0, v1);
}

GL_APICALL void GL_APIENTRY Sb_glUniform2iv(GLint location,
                                            GLsizei count,
                                            const GLint* value) {
  return SbGetGlesInterface()->glUniform2iv(location, count, value);
}

GL_APICALL void GL_APIENTRY Sb_glUniform3f(GLint location,
                                           GLfloat v0,
                                           GLfloat v1,
                                           GLfloat v2) {
  return SbGetGlesInterface()->glUniform3f(location, v0, v1, v2);
}

GL_APICALL void GL_APIENTRY Sb_glUniform3fv(GLint location,
                                            GLsizei count,
                                            const GLfloat* value) {
  return SbGetGlesInterface()->glUniform3fv(location, count, value);
}

GL_APICALL void GL_APIENTRY Sb_glUniform3i(GLint location,
                                           GLint v0,
                                           GLint v1,
                                           GLint v2) {
  return SbGetGlesInterface()->glUniform3i(location, v0, v1, v2);
}

GL_APICALL void GL_APIENTRY Sb_glUniform3iv(GLint location,
                                            GLsizei count,
                                            const GLint* value) {
  return SbGetGlesInterface()->glUniform3iv(location, count, value);
}

GL_APICALL void GL_APIENTRY
Sb_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
  return SbGetGlesInterface()->glUniform4f(location, v0, v1, v2, v3);
}

GL_APICALL void GL_APIENTRY Sb_glUniform4fv(GLint location,
                                            GLsizei count,
                                            const GLfloat* value) {
  return SbGetGlesInterface()->glUniform4fv(location, count, value);
}

GL_APICALL void GL_APIENTRY
Sb_glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
  return SbGetGlesInterface()->glUniform4i(location, v0, v1, v2, v3);
}

GL_APICALL void GL_APIENTRY Sb_glUniform4iv(GLint location,
                                            GLsizei count,
                                            const GLint* value) {
  return SbGetGlesInterface()->glUniform4iv(location, count, value);
}

GL_APICALL void GL_APIENTRY Sb_glUniformMatrix2fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  return SbGetGlesInterface()->glUniformMatrix2fv(location, count, transpose,
                                                  value);
}

GL_APICALL void GL_APIENTRY Sb_glUniformMatrix3fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  return SbGetGlesInterface()->glUniformMatrix3fv(location, count, transpose,
                                                  value);
}

GL_APICALL void GL_APIENTRY Sb_glUniformMatrix4fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  return SbGetGlesInterface()->glUniformMatrix4fv(location, count, transpose,
                                                  value);
}

GL_APICALL void GL_APIENTRY Sb_glUseProgram(GLuint program) {
  return SbGetGlesInterface()->glUseProgram(program);
}

GL_APICALL void GL_APIENTRY Sb_glValidateProgram(GLuint program) {
  return SbGetGlesInterface()->glValidateProgram(program);
}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib1f(GLuint index, GLfloat x) {
  return SbGetGlesInterface()->glVertexAttrib1f(index, x);
}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib1fv(GLuint index,
                                                 const GLfloat* v) {
  return SbGetGlesInterface()->glVertexAttrib1fv(index, v);
}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib2f(GLuint index,
                                                GLfloat x,
                                                GLfloat y) {
  return SbGetGlesInterface()->glVertexAttrib2f(index, x, y);
}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib2fv(GLuint index,
                                                 const GLfloat* v) {
  return SbGetGlesInterface()->glVertexAttrib2fv(index, v);
}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib3f(GLuint index,
                                                GLfloat x,
                                                GLfloat y,
                                                GLfloat z) {
  return SbGetGlesInterface()->glVertexAttrib3f(index, x, y, z);
}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib3fv(GLuint index,
                                                 const GLfloat* v) {
  return SbGetGlesInterface()->glVertexAttrib3fv(index, v);
}

GL_APICALL void GL_APIENTRY
Sb_glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  return SbGetGlesInterface()->glVertexAttrib4f(index, x, y, z, w);
}

GL_APICALL void GL_APIENTRY Sb_glVertexAttrib4fv(GLuint index,
                                                 const GLfloat* v) {
  return SbGetGlesInterface()->glVertexAttrib4fv(index, v);
}

GL_APICALL void GL_APIENTRY Sb_glVertexAttribPointer(GLuint index,
                                                     GLint size,
                                                     GLenum type,
                                                     GLboolean normalized,
                                                     GLsizei stride,
                                                     const void* pointer) {
  return SbGetGlesInterface()->glVertexAttribPointer(
      index, size, type, normalized, stride, pointer);
}

GL_APICALL void GL_APIENTRY Sb_glViewport(GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height) {
  return SbGetGlesInterface()->glViewport(x, y, width, height);
}

}  // extern "C"
